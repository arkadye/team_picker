#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <map>
#include <optional>
#include <limits>
#include <charconv>
#include <limits>
#include <numeric>
#include <array>
#include <format>
#include <filesystem>
#include <ranges>

using IST = std::istream_iterator<std::string>;

struct PositionRequirements
{
	std::vector<std::string> attacking, defensive;
	std::map<std::string, std::string> position_to_calculation;
	std::map<std::string, int> set_values;
	std::map<std::string, int> stat_defaults;
};

std::string_view trim_whitespace(std::string_view in)
{
	while (!in.empty() && isspace(in.front()))
	{
		in.remove_prefix(1);
	}
	while (!in.empty() && isspace(in.back()))
	{
		in.remove_suffix(1);
	}
	return in;
}

bool cicmp(std::string_view l, std::string_view r)
{
	return std::ranges::equal(l, r, {}, ::toupper, ::toupper);
};

struct Player
{
	std::string name;
	std::map<std::string, int> stats;
};

std::vector<std::string> read_header(std::istream& is)
{
	std::string header_line_data;
	std::getline(is, header_line_data);
	std::istringstream header_line{ header_line_data };
	{
		std::string name_str;
		header_line >> name_str;
		assert(name_str == "NAME");
	}

	std::vector<std::string> result;
	std::copy(IST{ header_line }, IST{}, std::back_inserter(result));
	return result;
}

Player get_player(std::istream& is, const std::vector<std::string>& stats, const PositionRequirements& position_requirements)
{
	Player result;
	is >> std::ws;
	std::getline(is, result.name);

	// Skip player number [#00]
	std::string number;
	is >> number;
	is >> std::ws;

	std::string stats_str;
	std::getline(is, stats_str);
	
	std::istringstream stats_data{ stats_str };
	while (!std::isdigit(stats_data.peek()))
	{
		const char first_char = stats_data.peek();
		if (std::isspace(first_char))
		{
			stats_data >> std::ws;
		}
		else if (std::isalpha(first_char) || (first_char == '[') || (first_char == ']') || (first_char == '(' || first_char == ')'))
		{
			std::string scratch;
			stats_data >> scratch;
		}
		else
		{
			const char bad_char = first_char;
			std::cout << std::format("    Choked processing digit {}['{}'] in '{}'\n", int{ bad_char }, bad_char, stats_str);
			return Player{};
		}
	}

	auto get_stat = [&value_conversions = position_requirements.set_values](std::string_view data)
		{
			{
				int result{};
				const auto conversion_result = std::from_chars(data.data(), data.data() + data.size(), result);
				if (conversion_result.ec == std::errc{})
				{
					return result;
				}
			}

			auto find_result = value_conversions.find(std::string{ data });
			assert(find_result != end(value_conversions) && "Could not find data field in value conversions");
			return find_result->second;
		};

	std::string stat;
	for (const std::string& stat_name : stats)
	{
		assert(!stats_data.eof());
		stats_data >> stat;
		result.stats.emplace(stat_name, get_stat(stat));
	}

	for (const auto& [stat, default_value] : position_requirements.stat_defaults)
	{
		if (!result.stats.contains(stat))
		{
			result.stats.emplace(stat, default_value);
		}
	}

	std::cout << std::format("    Read in player {}\n", result.name);
	return result;
}

std::vector<Player> get_roster(std::istream& input, const PositionRequirements& position_requirements)
{
	std::stringstream preprocessed;

	bool first = true;

	// Remove blank lines
	while (!input.eof())
	{
		std::string line;
		std::getline(input, line);
		std::ranges::transform(line, begin(line),::toupper);
		if (!(line.empty() || std::ranges::all_of(line, ::isspace)))
		{
			preprocessed << (first ? "" : "\n") << line;
			first = false;
		}
	}

	preprocessed.seekg(0);

	std::vector<Player> result;
	const std::vector<std::string> header = read_header(preprocessed);
	while (!preprocessed.eof())
	{
		Player new_player = get_player(preprocessed, header, position_requirements);
		result.push_back(std::move(new_player));
	}
	return result;
}

struct ForcedPositions
{
	std::vector<std::string> forced_player;
	bool contains(std::string_view name) const noexcept
	{
		auto find_result = std::ranges::find(forced_player, name);
		return find_result != end(forced_player);
	}
};

std::pair<PositionRequirements,ForcedPositions> parse_position_requirements(std::istream& iss)
{
	PositionRequirements result;
	ForcedPositions forced_positions;
	std::string post_step;
	while (!iss.eof())
	{
		std::string line_data;
		std::getline(iss, line_data);
		std::ranges::transform(line_data, begin(line_data), ::toupper);
		std::string_view line = line_data;
		if (line.empty() || line.front() == '#')
		{
			continue;
		}

		const auto eq_pos = line.find('=');
		if (eq_pos < line.size())
		{
			std::string_view val = trim_whitespace(line.substr(0, eq_pos));
			std::string_view calculation = trim_whitespace(line.substr(eq_pos + 1));
			result.position_to_calculation.insert(std::pair{ std::string{val},std::string{calculation} });
			continue;
		}

		const auto colon_pos = line.find(':');
		if (colon_pos < line.size())
		{
			enum class LineType
			{
				UNINITIALIZED,
				OFFENCE,
				DEFENCE,
				FORCE,
				SET,
				POST,
				INITIALIZE
			};

			std::string_view prefix = trim_whitespace(line.substr(0, colon_pos));
			std::string_view arg = trim_whitespace(line.substr(colon_pos + 1));

			LineType status = LineType::UNINITIALIZED;
			switch (prefix.front())
			{
			case 'o':
			case 'O':
				status = LineType::OFFENCE;
				assert(result.attacking.empty());
				break;
			case 'd':
			case 'D':
				status = LineType::DEFENCE;
				assert(result.defensive.empty());
				break;
			case 'f':
			case 'F':
				status = LineType::FORCE;
				break;
			case 'S':
			case 's':
				status = LineType::SET;
				break;
			case 'P':
			case 'p':
				status = LineType::POST;
				assert(post_step.empty());
				break;
			case 'I':
			case 'i':
				status = LineType::INITIALIZE;
				break;
			default:
				break;
			}

			auto parse_positions = [arg, colon_pos](std::vector<std::string>& target)
				{
					std::istringstream positions{ std::string{ arg } };
					std::ranges::copy(std::views::istream<std::string>(positions), std::back_inserter(target));
				};

			auto parse_set_values = [arg]()
				{
					std::istringstream positions{ std::string{ arg } };
					std::string key;
					int value{};
					positions >> key >> value;
					return std::pair<const std::string, int>{ key , value };
				};

			switch (status)
			{
			case LineType::UNINITIALIZED:
				break;
			case LineType::OFFENCE:
				parse_positions(result.attacking);
				break;
			case LineType::DEFENCE:
				parse_positions(result.defensive);
				break;
			case LineType::FORCE:
				forced_positions.forced_player.emplace_back(arg);
				break;
			case LineType::POST:
				post_step = arg;
				break;
			case LineType::SET:
				result.set_values.insert(parse_set_values());
				break;
			case LineType::INITIALIZE:
				result.stat_defaults.insert(parse_set_values());
				break;
			}
		}
	}

	if (!post_step.empty())
	{
		for (auto& [position,calculation] : result.position_to_calculation)
		{
			calculation = std::format("({}) {}", calculation, post_step);
		}
	}

	return std::make_pair(std::move(result), std::move(forced_positions));
}

double evaluate_player(const Player& player, std::string_view calculation);

double evaluate_player_op(const Player& player, std::string_view calculation, std::size_t op_pos, std::size_t op_len)
{
	std::string_view left = calculation.substr(0, op_pos);
	std::string_view op = calculation.substr(op_pos, op_len);
	std::string_view right = calculation.substr(op_pos + op_len);	
	const double l = evaluate_player(player, left);
	const double r = evaluate_player(player, right);

	if (op == "+") return l + r;
	if (op == "-") return l - r;
	if (op == "*") return l * r;
	if (op == "/") return l / r;
	if (op == "^") return std::pow(l, r);;
	if (op == ">") return l > r ? 1.0 : 0.0;
	if (op == "<") return l < r ? 1.0 : 0.0;
	if (op == ">=") return l >= r ? 1.0 : 0.0;
	if (op == "<=") return l <= r ? 1.0 : 0.0;
	if (op == "==") return std::abs(l - r) < 0.000001 ? 1.0 : 0.0;
	if (op == "!=") return std::abs(l - r) >= 0.000001 ? 1.0 : 0.0;

	const bool bl = std::abs(l) > 0.5;
	const bool br = std::abs(r) > 0.5;

	if (op == "&&") return bl && br;
	if (op == "||") return bl || br;
	assert(false);
	return 0.0;
}

double evaluate_player(const Player& player, std::string_view calculation)
{
	assert(!calculation.empty());
	assert(!std::ranges::all_of(calculation, ::isspace));
	calculation = trim_whitespace(calculation);

	{
		double result_val = 0.0;
		std::from_chars_result parse_result = std::from_chars(calculation.data(), calculation.data() + calculation.size(), result_val);
		if (parse_result.ec == std::errc{} && parse_result.ptr == (calculation.data() + calculation.size()))
		{
			return result_val;
		}
	}

	bool fully_in_brackets = true;
	int bracket_depth = 0;

	constexpr std::array<std::string_view, 13> ops = {
		"||", "&&","<<",">>","<",">","==","!=", "+", "-", "*", "/","^"
	};
	std::array<std::optional<std::size_t>, ops.size()> found_ops;
	bool found_op = false;

	for (std::size_t cal_i = 0; cal_i < calculation.size(); ++cal_i)
	{
		const char c = calculation[cal_i];
		bool done = true;
		switch (c)
		{
		case '(':
			++bracket_depth;
			break;
		case ')':
			assert(bracket_depth > 0);
			--bracket_depth;
			break;
		default:
			done = false;
			break;
		}

		if (done || (bracket_depth > 0))  continue;

		fully_in_brackets = false;
		const std::string_view partial_calc = calculation.substr(cal_i);
		for (std::size_t op_i = 0u;op_i < ops.size();++op_i)
		{
			if (partial_calc.size() >= ops[op_i].size() && partial_calc.starts_with(ops[op_i]))
			{
				found_ops[op_i] = cal_i;
				found_op = true;
				break;
			}
		}

	}
	assert(bracket_depth == 0);
	if (fully_in_brackets)
	{
		calculation.remove_prefix(1);
		calculation.remove_suffix(1);
		return evaluate_player(player, calculation);
	}

	for (std::size_t i = 0u; i < found_ops.size(); ++i)
	{
		if (found_ops[i].has_value())
		{
			return evaluate_player_op(player, calculation, found_ops[i].value(), ops[i].size());
		}
	}

	for (const auto& [stat, val] : player.stats)
	{
		if (stat == calculation) return static_cast<double>(val);
	}

	std::array<std::string_view, 5> functions{
		"MIN",
		"MAX",
		"IF",
		"POW",
		"AVERAG£"
	};

	auto find_result = std::ranges::find_if(functions, [calculation](std::string_view fn)
		{
			if (calculation.size() < fn.size()) return false;
			std::string_view prefix = calculation.substr(0, fn.size());
			return prefix == fn;
		});

	if (find_result == end(functions))
	{
		return 0.0;
	}

	std::string_view function = *find_result;
	auto get_args = [](std::string_view args) ->  std::vector<std::string_view>
		{
			std::vector<std::string_view> result;
			const std::size_t bracket_start = args.find_first_of('(');
			assert(bracket_start < args.size());
			args = args.substr(bracket_start + 1);
			const std::size_t bracket_end = args.find_last_of(')');
			args = args.substr(0, bracket_end);

			bool has_any_nonwhitespace = false;
			while (!args.empty())
			{
				int bracket_depth = 0;
				std::size_t i = 0u;
				for (; i < args.size(); ++i)
				{
					const char c = args[i];
					bool found_end = false;
					if (!isspace(c)) has_any_nonwhitespace = true;
					switch (c)
					{
					case '(':
						++bracket_depth;
						break;
					case ')':
						assert(bracket_depth > 0);
						--bracket_depth;
						break;
					case ',':
						if (bracket_depth == 0)
						{
							found_end = true;
						}
						break;
					}
					if (found_end) break;
				}
				assert(bracket_depth == 0);
				result.push_back(args.substr(0, i));
				args = (i < args.size()) ? args.substr(i + 1) : std::string_view{};
			}
			if (!has_any_nonwhitespace) result.clear();
			return result;
		};

	const std::vector<std::string_view> fn_args = get_args(calculation.substr(function.size()));

	auto eval = [&player](std::string_view expr) { return evaluate_player(player, expr); };
	if (function == "MIN")
	{
		if (fn_args.empty()) return 0.0f;
		double result = std::numeric_limits<double>::max();
		for (std::string_view expr : fn_args)
		{
			result = std::min(result, eval(expr));
		}
		return result;
	}
	if (function == "MAX")
	{
		if (fn_args.empty()) return 0.0f;
		double result = std::numeric_limits<double>::min();
		for (std::string_view expr : fn_args)
		{
			result = std::max(result, eval(expr));
		}
		return result;
	}
	if (function == "IF")
	{
		assert(fn_args.size() == 3u);
		return eval(std::abs(eval(fn_args[0])) > 0.5 ? fn_args[1] : fn_args[2]);
	}
	if (function == "POW")
	{
		assert(fn_args.size() == 2u);
		return std::pow(eval(fn_args[0]), eval(fn_args[1]));
	}
	if (function == "AVERAGE")
	{
		assert(fn_args.size() > 0);
		return std::transform_reduce(begin(fn_args), end(fn_args), 0.0, std::plus<double>{}, eval) / static_cast<double>(fn_args.size());
	}
	assert(false && "Failed to evaluate");
	return 0.0f;
}

double evaluate_player(const Player& player, const std::string& position, const PositionRequirements& requirements)
{
	std::string_view calculation = position;
	auto calc_it = requirements.position_to_calculation.find(position);
	if (calc_it != end(requirements.position_to_calculation))
	{
		calculation = calc_it->second;
	}
	return evaluate_player(player, calculation);
}

struct RosterPosition
{
	std::string name, offence, defence;
	double offensive_score = 0.0;
	double defensive_score = 0.0;
	double total_score = 0.0;
};

struct PickTempData
{
	std::string_view name;
	std::map<std::string_view, double> position_scores;
	double max_score = 0.0f;
};

auto operator==(const PickTempData& l, const PickTempData& r) noexcept { return l.name == r.name; }

PickTempData to_pick_data(const Player& p, const PositionRequirements& requirements)
{
	PickTempData r;
	r.name = p.name;
	double max_offence = std::numeric_limits<double>::min();
	double max_defence = std::numeric_limits<double>::min();
	auto add_scores = [&r, &requirements, &p](const std::vector<std::string>& positions, double& max)
		{
			for (const std::string& pos : positions)
			{
				if (!r.position_scores.contains(pos))
				{
					const double score = evaluate_player(p, pos, requirements);
					r.position_scores.insert(std::pair{ std::string_view{ pos }, score });
					max = std::max(max, score);
				}
			}
		};
	add_scores(requirements.attacking, max_offence);
	add_scores(requirements.defensive, max_defence);
	r.max_score = max_offence + max_defence;
	std::cout << std::format("    Evaluating {}\n", r.name);
	return r;
}

struct PositionDescription
{
	std::string_view position;
	double score = 0.0;
};

struct StartingPositionDescription
{
	std::string_view name;
	PositionDescription offence, defence;
	double score = 0.0f;
};

template <typename ItType>
std::pair<std::vector<std::pair<std::string_view, PositionDescription>>, double> find_best_positions(
	ItType first,
	ItType last,
	const std::vector<std::string_view>& positions)
{
	assert(std::distance(first, last) == positions.size());
	if (first == last)
	{
		return std::make_pair(std::vector<std::pair<std::string_view, PositionDescription>>{}, 0.0);
	}

	std::vector<std::string_view> checked_positions;
	checked_positions.reserve(positions.size());

	std::vector<std::pair<std::string_view, PositionDescription>> result;
	PositionDescription best_position;
	double best_overall_score = std::numeric_limits<double>::min();
	const PickTempData& data = *first;
	for (std::string_view pos : positions)
	{
		if (data.position_scores.contains(pos))
		{
			if (std::ranges::find(checked_positions, pos) == end(checked_positions))
			{
				checked_positions.push_back(pos);
				const double position_score = data.position_scores.find(pos)->second;
				auto positions_copy = positions;
				positions_copy.erase(std::ranges::find(positions_copy, pos));
				auto [new_result, new_score] = find_best_positions(first + 1, last, positions_copy);
				const double total = position_score + new_score;
				if (total > best_overall_score)
				{
					result = std::move(new_result);
					best_position.position = pos;
					best_position.score = position_score;
					best_overall_score = total;
				}
			}
		}
	}
	result.emplace_back(data.name, std::move(best_position));
	return std::pair{ std::move(result), best_overall_score };
}

template <typename ItType>
std::pair<std::vector<std::pair<std::string_view, PositionDescription>>, double> find_best_positions(
	ItType first,
	ItType last,
	const std::vector<std::string>& positions)
{
	std::vector<std::string_view> svpos(begin(positions), end(positions));
	return find_best_positions(first, last, svpos);
}

template <typename RangeType>
std::pair<std::vector<std::pair<std::string_view, PositionDescription>>, double> find_best_positions(
	const RangeType& range,
	const std::vector<std::string>& positions)
{
	return find_best_positions(begin(range), end(range), positions);
}

template <typename ItType>
std::vector<StartingPositionDescription> make_line_up(
	ItType first,
	ItType last,
	const std::vector<std::pair<std::string_view, PositionDescription>>& attacking_lineup,
	const std::vector<std::pair<std::string_view, PositionDescription>>& defending_lineup)
{
	assert(attacking_lineup.size() == defending_lineup.size());
	std::vector<StartingPositionDescription> result;
	result.reserve(attacking_lineup.size());

	auto find_data = [](std::string_view name, const std::vector<std::pair<std::string_view, PositionDescription>>& score)
		{
			const auto result = std::ranges::find(score, name, [](const std::pair<std::string_view, PositionDescription>& p) {return p.first; });
			assert(result != end(score));
			return result->second;
		};

	auto make_starting_position_data = [&find_data, &attacking_lineup, &defending_lineup](const PickTempData& starter)
		{
			StartingPositionDescription r;
			r.name = starter.name;
			r.offence = find_data(starter.name, attacking_lineup);
			r.defence = find_data(starter.name, defending_lineup);
			r.score = r.offence.score + r.defence.score;
			return r;
		};

	std::transform(first, last, std::back_inserter(result), make_starting_position_data);
	return result;
}

template <typename RangeType>
std::vector<StartingPositionDescription> make_line_up(
	const RangeType& line_up,
	const std::vector<std::pair<std::string_view, PositionDescription>>& attacking_lineup,
	const std::vector<std::pair<std::string_view, PositionDescription>>& defending_lineup)

{
	return make_line_up(begin(line_up), end(line_up), attacking_lineup, defending_lineup);
}

std::pair<std::vector<StartingPositionDescription>, double> get_initial_try_starters(const std::vector<PickTempData>& data, const PositionRequirements& requirements)
{
	std::cout << "Picking initial starting line up...\n";
	assert(requirements.attacking.size() == requirements.defensive.size());
	const std::size_t target_size = requirements.attacking.size();
	assert(data.size() >= target_size);

	auto first = begin(data);
	auto last = first + target_size;

	const auto [attacking_lineup, attacking_score] = find_best_positions(first, last, requirements.attacking);
	const auto [defending_lineup, defending_score] = find_best_positions(first, last, requirements.defensive);
	const double total_score = attacking_score + defending_score;

	std::vector<StartingPositionDescription> result = make_line_up(first, last, attacking_lineup, defending_lineup);
	return std::pair{ std::move(result), total_score };
}

std::optional<std::pair<std::vector<StartingPositionDescription>, double>> try_swapping_in_player_for_player(
	std::vector<const PickTempData*> picks,			// Current starting line up
	const PickTempData& player_in,					// Player in
	const PickTempData& player_out,					// Player out
	const PositionRequirements& requirements,		// Position requirements
	const ForcedPositions& forced_picks,			// Player who must be in the line up
	double current_best								// Current best evaluation
)
{
	if (forced_picks.contains(player_out.name))
	{
		return std::nullopt;
	}

	auto picks_view = picks | std::ranges::views::transform([](const PickTempData* e) {return *e; });
	assert(std::ranges::find(picks, &player_in) == end(picks));

	const auto pick_out_it = std::ranges::find(picks, &player_out);
	assert(pick_out_it != end(picks));

	*pick_out_it = &player_in;

	const auto [attacking_lineup, attacking_score] = find_best_positions(picks_view, requirements.attacking);
	const auto [defending_lineup, defending_score] = find_best_positions(picks_view, requirements.defensive);
	const double total_score = attacking_score + defending_score;

	if (total_score > current_best)
	{
		std::vector<StartingPositionDescription> line_up = make_line_up(picks_view, attacking_lineup, defending_lineup);
		return std::pair{ std::move(line_up), total_score };
	}
	return std::nullopt;
}

std::optional<std::pair<std::vector<StartingPositionDescription>, double>> try_swapping_in_player(
	const std::vector<const PickTempData*>& picks,	// Current starting line up
	const PickTempData& player_in,					// Player in
	const PositionRequirements& requirements,		// Position requirements
	const ForcedPositions& forced_picks,			// Player who must be in the line up
	double current_best								// Current best scores
)
{
	// If this is a player already in the starting lineup, skip them.
	if (std::ranges::find(picks, &player_in) != end(picks))
	{
		return std::nullopt;
	}

	const double original_score = current_best;

	// This player WILL end up on the team, one way or another.
	if (forced_picks.contains(player_in.name))
	{
		current_best = std::numeric_limits<double>::min();
	}

	std::optional<std::pair<std::vector<StartingPositionDescription>, double>> result;
	std::string_view player_out;
	for (const PickTempData* starter : picks)
	{
		if (auto swap_result_optional = try_swapping_in_player_for_player(picks, player_in, *starter, requirements, forced_picks, current_best))
		{
			auto& [new_picks, new_best] = *swap_result_optional;
			std::cout << std::format("     Score improvement replacing {}: {}\n", starter->name, (new_best - original_score));
			assert(new_best > current_best);
			current_best = new_best;
			result = std::move(*swap_result_optional);
			player_out = starter->name;
		}
	}

	if (result.has_value())
	{
		std::cout << std::format("    Swapped in {} replacing {}\n", player_in.name, player_out);
	}
	return result;
}

std::vector<PickTempData> make_pick_temp_data(const std::vector<Player>& roster, const PositionRequirements& requirements)
{
	std::vector<PickTempData> pick_data;
	pick_data.reserve(roster.size());
	std::ranges::transform(roster, std::back_inserter(pick_data), [&r = requirements](const Player& p) {return to_pick_data(p, r); });
	std::ranges::sort(pick_data, {}, [](const PickTempData& ptd) {return -ptd.max_score; });
	return pick_data;
}

std::vector<const PickTempData*> make_starter_data(const std::vector<StartingPositionDescription>& starters, const std::vector<PickTempData>& roster)
{
	std::vector<const PickTempData*> starter_data;
	starter_data.reserve(starters.size());
	std::ranges::transform(starters, std::back_inserter(starter_data), [&roster](const StartingPositionDescription& starter)
		{
			auto find_result = std::ranges::find(roster, starter.name, &PickTempData::name);
			assert(find_result != end(roster));
			return &(*find_result);
		});
	return starter_data;
}

std::pair<std::vector<StartingPositionDescription>, double> pick_team(const std::vector<PickTempData>& roster, const PositionRequirements& requirements, const ForcedPositions& forced_picks)
{
	auto [starters, best_score] = get_initial_try_starters(roster, requirements);
	std::ranges::sort(starters, {}, &StartingPositionDescription::score);

	bool has_made_change = true;
	int changes_tried = 0;
	while (has_made_change)
	{
		has_made_change = false;

		const std::vector<const PickTempData*> starter_data = make_starter_data(starters, roster);

		for (const PickTempData& trial_player : roster)
		{
			std::cout << std::format("{}: trying {} as a starter.\n", changes_tried++, trial_player.name);
			if (auto swap_result_opt = try_swapping_in_player(starter_data, trial_player, requirements, forced_picks, best_score))
			{
				std::cout << "    Swap made. Restarting.\n";
				auto& [new_starters, new_score] = *swap_result_opt;
				assert(new_score > best_score || forced_picks.contains(trial_player.name));
				best_score = new_score;
				starters = std::move(new_starters);
				has_made_change = true;
				break;
			}
		}
	}
	return { starters, best_score };
}

RosterPosition make_roster_position(const StartingPositionDescription& spd)
{
	RosterPosition r;
	r.name = std::string{ spd.name };
	r.offence = std::string{ spd.offence.position };
	r.defence = std::string{ spd.defence.position };
	r.offensive_score = spd.offence.score;
	r.defensive_score = spd.defence.score;
	r.total_score = spd.score;
	return r;
}

std::vector<RosterPosition> pick_team(const std::vector<Player>& roster, const PositionRequirements& requirements, const ForcedPositions& forced_picks)
{
	const std::vector<PickTempData> pick_data = make_pick_temp_data(roster, requirements);

	auto [starters, best_score] = pick_team(pick_data, requirements, forced_picks);
	std::ranges::sort(starters, {}, &StartingPositionDescription::score);

	std::vector<RosterPosition> result;
	result.reserve(starters.size());
	std::ranges::transform(starters, std::back_inserter(result), make_roster_position);
	return result;
}

RosterPosition evaluate_draftee(const std::vector<const PickTempData*> starter_data, const Player& evaluee, const PositionRequirements& requirements, ForcedPositions& forced_picks)
{
	const PickTempData evaluee_data = to_pick_data(evaluee, requirements);
	forced_picks.forced_player.push_back(evaluee.name);
	auto swap_in_result_opt = try_swapping_in_player(starter_data, evaluee_data, requirements, forced_picks, 0.0);
	forced_picks.forced_player.pop_back();

	assert(swap_in_result_opt.has_value());
	const auto& [team_result, squad_strength] = *swap_in_result_opt;
	auto evaluee_result_it = std::ranges::find(team_result, evaluee.name, &StartingPositionDescription::name);
	assert(evaluee_result_it != end(team_result));
	RosterPosition result = make_roster_position(*evaluee_result_it);
	result.total_score = squad_strength;
	return result;
}

std::vector<RosterPosition> evaluate_all_draftees(const std::vector<Player>& roster, const std::vector<Player>& draft_class, const PositionRequirements& requirements, const ForcedPositions& forced_picks)
{
	const std::vector<PickTempData> roster_pick_data = make_pick_temp_data(roster, requirements);

	const auto [starters, starters_score] = pick_team(roster_pick_data, requirements, forced_picks);

	const std::vector<const PickTempData*> starter_data = make_starter_data(starters, roster_pick_data);

	auto mutable_forced_picks = forced_picks;
	auto evaluate = [&starter_data, &requirements, &mutable_forced_picks, starters_score](const Player& player)
		{
			RosterPosition result = evaluate_draftee(starter_data, player, requirements, mutable_forced_picks);
			result.total_score -= starters_score;
			return result;
		};

	std::vector<RosterPosition> draft_pick_data;
	draft_pick_data.reserve(draft_class.size());
	std::ranges::transform(draft_class, std::back_inserter(draft_pick_data), evaluate);
	std::ranges::sort(draft_pick_data, {}, [](const RosterPosition& rp) {return -rp.total_score; });
	return draft_pick_data;
}

void run_pick_team_mode(const std::vector<Player>& roster, const PositionRequirements& requirements, const ForcedPositions& forced_positions)
{
	std::cout << "Picking the team...\n";
	std::vector<RosterPosition> picks = pick_team(roster, requirements, forced_positions);

	std::cout << "\nTEAM PICKED:\n";

	std::ranges::sort(picks, std::greater<double>{}, [](const RosterPosition& rp) {return rp.total_score; });

	std::vector<RosterPosition> output;
	output.reserve(picks.size());

	for (std::string_view pos : requirements.attacking)
	{
		auto pick_it = std::ranges::find(picks, pos, [](const RosterPosition& rp) {return rp.offence; });
		assert(pick_it != end(picks));
		output.push_back(std::move(*pick_it));
		picks.erase(pick_it);
	}

	const std::size_t max_name_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.name.size(); }).name.size();
	const std::size_t max_off_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.offence.size(); }).offence.size();
	const std::size_t max_def_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.defence.size(); }).defence.size();

	double team_offensive_score = 0;
	double team_defensive_score = 0;
	double team_total_score = 0;

	for (const RosterPosition& pick : output)
	{
		std::cout << std::format("{:{}} / {:{}} - {:{}} {:.0f} + {:.0f} = {:.0f}\n",
			pick.offence, max_off_len,
			pick.defence, max_def_len,
			pick.name, max_name_len,
			pick.offensive_score,
			pick.defensive_score,
			pick.total_score
		);

		team_offensive_score += pick.offensive_score;
		team_defensive_score += pick.defensive_score;
		team_total_score += pick.total_score;
	}

	std::cout << std::format("\n     Team total: {:.0f} + {:.0f} = {:.0f}\n\n",
		team_offensive_score,
		team_defensive_score,
		team_total_score
	);
}

void run_evaluate_draft_mode(const std::vector<Player>& roster, const std::vector<Player> draft_class, const PositionRequirements& requirements, const ForcedPositions& forced_positions)
{
	const std::vector<RosterPosition> output = evaluate_all_draftees(roster, draft_class, requirements, forced_positions);
	const std::size_t max_name_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.name.size(); }).name.size();
	const std::size_t max_off_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.offence.size(); }).offence.size();
	const std::size_t max_def_len = std::ranges::max(output, {}, [](const RosterPosition& rp) {return rp.defence.size(); }).defence.size();

	std::cout << "DRAFTEE EVALUATIONS\n";
	for (const RosterPosition& pick : output)
	{
		std::cout << std::format("{:{}} / {:{}} - {:{}} {:.0f} + {:.0f} = {:.0f}\n",
			pick.offence, max_off_len,
			pick.defence, max_def_len,
			pick.name, max_name_len,
			pick.offensive_score,
			pick.defensive_score,
			pick.total_score
		);
	}

	std::cout << "\n\nPICKS\n||";
	for (const RosterPosition& pick : output | std::views::take(32))
	{
		std::cout << std::format("`{:{}}`\n", pick.name, max_name_len);
	}
	std::cout << "||\n";
}

int main(int argc, char** argv)
{
	auto quit = []()
		{
			std::cout << "Press 'Enter' to quit.";
			std::cin.get();
			exit(0);
		};

	std::cout << "Reading command line args\n";
	std::filesystem::path team_data{ "team_data.txt" };
	std::filesystem::path composition{ "composition.txt" };
	std::filesystem::path draft_class;
	if (std::filesystem::exists("draft_class.txt"))
	{
		draft_class = "draft_class.txt";
	}
	{
		enum class ArgState
		{
			NotFound,
			Next,
			Found
		};
		ArgState td_state = ArgState::NotFound;
		ArgState cmp_state = ArgState::NotFound;
		ArgState df_state = ArgState::NotFound;
		for (int i = 1; i < argc; ++i)
		{
			std::string_view arg{ argv[i] };
			if (cicmp(arg, "--help") || cicmp(arg, "-help") || cicmp(arg, "help"))
			{
				std::cout << "Team Picker by arkadye.\n"
					"Usage: arguments optional.\n"
					"    --team-data [path]: a path to a team data file\n"
					"    --composition [path]: a path to a composition file\n"
					"    --draft-class [path]: [OPTIONAL] a path to a list of potential signings. Puts team picker in draft mode."
					"For more info and latest versions visit https://github.com/arkadye/team_picker\n";
				quit();
			}
			auto handle_arg = [arg, &quit](std::filesystem::path& target, ArgState& state, std::string_view match)
				{
					if (state == ArgState::Next)
					{
						target = arg;
						state = ArgState::Next;
						return;
					}

					if (cicmp(arg, match))
					{
						if (state == ArgState::Found)
						{
							std::cout << "Multiple " << match << " arguments found!\n";
							quit();
						}
						state = ArgState::Next;
					}
				};

			handle_arg(team_data, td_state, "--team-data");
			handle_arg(composition, cmp_state, "--composition");
			handle_arg(draft_class, df_state, "--draft-class");
		}
	}

	std::cout << "Loading " << composition << '\n';
	std::ifstream req_input{ composition };
	if (!req_input.is_open())
	{
		std::cout << std::format("Could not open {}\n", composition.generic_string());
		quit();
	}
	const auto [requirements , forced_positions] = parse_position_requirements(req_input);

	std::cout << "Loading " << team_data << '\n';
	std::ifstream team_input{ team_data };
	if (!team_input.is_open())
	{
		std::cout << std::format("Could not open {}\n", team_data.generic_string());
		quit();
	}
	const std::vector<Player> roster = get_roster(team_input, requirements);

	std::vector<Player> draft_roster;
	if (!draft_class.empty())
	{
		std::cout << "Loading " << draft_class << '\n';
		std::ifstream draft_input{ draft_class };
		if (!draft_input.is_open())
		{
			std::cout << std::format("Could not open {}\n", draft_class.generic_string());
			quit();
		}
		draft_roster = get_roster(draft_input, requirements);
	}

	if (draft_roster.empty())
	{
		run_pick_team_mode(roster, requirements, forced_positions);
	}
	else
	{
		run_evaluate_draft_mode(roster, draft_roster, requirements, forced_positions);
	}
	quit();
}
