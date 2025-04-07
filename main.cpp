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

using IST = std::istream_iterator<std::string>;

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
		assert(name_str == "Name");
	}

	std::vector<std::string> result;
	std::copy(IST{ header_line }, IST{}, std::back_inserter(result));
	return result;
}

Player get_player(std::istream& is, const std::vector<std::string>& stats)
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
		if (std::isspace(stats_data.peek()))
		{
			stats_data >> std::ws;
		}
		else if (std::isalpha(stats_data.peek()))
		{
			std::string scratch;
			stats_data >> scratch;
		}
	}

	for (const std::string& stat_name : stats)
	{
		assert(!stats_data.eof());
		int stat = 0;
		stats_data >> stat;
		result.stats.insert(std::pair{ stat_name, stat });
	}
	std::cout << std::format("    Read in player {}\n", result.name);
	return result;
}

std::vector<Player> get_roster(std::istream& input)
{
	std::stringstream preprocessed;

	bool first = true;

	// Remove blank lines
	while (!input.eof())
	{
		std::string line;
		std::getline(input, line);
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
		Player new_player = get_player(preprocessed, header);
		result.push_back(std::move(new_player));
	}
	return result;
}

struct PositionRequirements
{
	std::vector<std::string> attacking, defensive;
	std::map<std::string, std::string> position_to_calculation;
};

PositionRequirements parse_position_requirements(std::istream& iss)
{
	PositionRequirements result;
	while (!iss.eof())
	{
		std::string line_data;
		std::getline(iss, line_data);
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
			constexpr int UNINTIALIZED = 0;
			constexpr int OFFENCE = 1;
			constexpr int DEFENCE = 2;

			std::string_view prefix = trim_whitespace(line.substr(0, colon_pos));
			int status = UNINTIALIZED;
			switch (prefix.front())
			{
			case 'o':
			case 'O':
				status = OFFENCE;
				assert(result.attacking.empty());
				break;
			case 'd':
			case 'D':
				status = DEFENCE;
				assert(result.defensive.empty());
				break;
			default:
				break;
			}

			if (status == UNINTIALIZED) continue;

			std::istringstream positions{ std::string{ line.substr(colon_pos + 1) } };
			std::vector<std::string>& target = status == OFFENCE ? result.attacking : result.defensive;
			std::copy(std::istream_iterator<std::string>{positions}, std::istream_iterator<std::string>{}, std::back_inserter(target));
		}
	}
	return result;
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
		if (cicmp(stat, calculation)) return static_cast<double>(val);
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
			return cicmp(prefix, fn);
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

std::pair<std::vector<std::pair<std::string_view, PositionDescription>>, double> find_best_positions(
	std::vector<PickTempData>::const_iterator first,
	std::vector<PickTempData>::const_iterator last,
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

std::pair<std::vector<std::pair<std::string_view, PositionDescription>>, double> find_best_positions(
	std::vector<PickTempData>::const_iterator first,
	std::vector<PickTempData>::const_iterator last,
	const std::vector<std::string>& positions)
{
	std::vector<std::string_view> svpos(begin(positions), end(positions));
	return find_best_positions(first, last, svpos);
}

std::pair<std::vector<StartingPositionDescription>, double> get_initial_try_starters(const std::vector<PickTempData>& data, const PositionRequirements& requirements)
{
	std::cout << "Picking initial starting line up...\n";
	const std::size_t target_size = requirements.attacking.size();
	assert(requirements.defensive.size() == target_size);
	assert(data.size() >= target_size);

	auto first = begin(data);
	auto last = first + target_size;

	auto [attacking_lineup, attacking_score] = find_best_positions(first, last, requirements.attacking);
	auto [defending_lineup, defending_score] = find_best_positions(first, last, requirements.defensive);
	const double total_score = attacking_score + defending_score;

	std::vector<StartingPositionDescription> result;
	result.reserve(target_size);

	for (const PickTempData& starter : std::ranges::subrange{ first,last })
	{
		auto find_data = [name = starter.name](const std::vector<std::pair<std::string_view, PositionDescription>>& score)
			{
				const auto result = std::ranges::find(score, name, [](const std::pair<std::string_view, PositionDescription>& p) {return p.first; });
				assert(result != end(score));
				return result->second;
			};

		StartingPositionDescription r;
		r.name = starter.name;
		r.offence = find_data(attacking_lineup);
		r.defence = find_data(defending_lineup);
		r.score = r.offence.score + r.defence.score;
		result.push_back(std::move(r));
	}
	return std::pair{ result, total_score };
}

std::pair<std::vector<StartingPositionDescription>, bool> try_swapping_in_player(std::vector<StartingPositionDescription> picks, const std::vector<PickTempData>& data, const PositionRequirements& requirements, const PickTempData& player)
{
	if (std::ranges::find(picks, player.name, [](const StartingPositionDescription& spd) {return spd.name; }) != end(picks))
	{
		return std::pair{ picks,false };
	}

	const double old_defence_score = std::transform_reduce(begin(picks), end(picks), 0.0, std::plus<double>{},
		[](const StartingPositionDescription& spd) {return spd.defence.score; });

	auto get_pick_data = [&data](std::string_view name) -> const PickTempData&
		{
			const auto find_result = std::ranges::find(data, name, [](const PickTempData& ptd) {return ptd.name; });
			assert(find_result != end(data));
			return *find_result;
		};

	std::vector<PickTempData> data_copy;
	data_copy.reserve(picks.size());
	std::transform(begin(picks), end(picks), std::back_inserter(data_copy),
		[&get_pick_data](const StartingPositionDescription& spd) {return get_pick_data(spd.name); });

	std::vector<StartingPositionDescription> best_improvement;
	std::string_view swapped_out_player;
	double best_delta = 0.0;

	for (std::size_t i = 0u; i < picks.size(); ++i)
	{
		if (player.max_score < picks[i].score) continue;
		StartingPositionDescription backup_spd = picks[i];
		PickTempData backup_ptd = data_copy[i];

		data_copy[i] = player;
		picks[i].name = player.name;
		picks[i].offence.score = player.position_scores.find(backup_spd.offence.position)->second;
		const double offence_delta = picks[i].offence.score - backup_spd.offence.score;
		auto [new_positions, new_score] = find_best_positions(begin(data_copy), end(data_copy), requirements.defensive);
		const double defence_delta = new_score - old_defence_score;
		const double change_delta = offence_delta + defence_delta;
		if (change_delta > best_delta)
		{
			best_improvement = picks;
			best_delta = change_delta;
			swapped_out_player = backup_spd.name;
			for (StartingPositionDescription& pick : best_improvement)
			{
				auto find_it = std::ranges::find(new_positions, pick.name, [](const auto& d) {return d.first; });
				assert(find_it != end(new_positions));
				pick.defence = find_it->second;
				pick.score = pick.offence.score + pick.defence.score;
			}
		}

		data_copy[i] = backup_ptd;
		picks[i] = backup_spd;
	}

	if (best_improvement.empty())
	{
		return std::pair{ picks, false };
	}

	for (StartingPositionDescription& spd : best_improvement)
	{
		const PickTempData& pdt = get_pick_data(spd.name);
		spd.defence.score = pdt.position_scores.find(spd.defence.position)->second;
		spd.score = spd.defence.score + spd.offence.score;
	}
	std::cout << std::format("    Swapped in {} replacing {}\n", player.name, swapped_out_player);

	return std::pair{ best_improvement, true };
}

std::vector<RosterPosition> pick_team(const std::vector<Player>& roster, const PositionRequirements& requirements)
{
	std::vector<PickTempData> pick_data;
	pick_data.reserve(roster.size());
	std::transform(begin(roster), end(roster), std::back_inserter(pick_data), [&r = requirements](const Player& p) {return to_pick_data(p, r); });
	std::ranges::sort(pick_data, {}, [](const PickTempData& ptd) {return ptd.max_score; });
	std::ranges::reverse(pick_data);

	auto [starters, best_score] = get_initial_try_starters(pick_data, requirements);

	bool has_made_change = true;
	int changes_tried = 0;
	while (has_made_change)
	{
		has_made_change = false;
		for (const PickTempData& trial_player : pick_data)
		{
			std::cout << std::format("{}: trying {} as a starter.\n", changes_tried++, trial_player.name);
			auto [new_starters, change_made] = try_swapping_in_player(starters, pick_data, requirements, trial_player);
			if (change_made)
			{
				std::cout << "    Swap made. Restarting.\n";
				const double new_score = std::transform_reduce(begin(new_starters), end(new_starters), 0.0, std::plus<double>{},
					[](const StartingPositionDescription& spd) {return spd.score; });
				assert(new_score > best_score);
				best_score = new_score;
				starters = std::move(new_starters);
				has_made_change = true;
				break;
			}
		}
	}
	std::vector<RosterPosition> result;
	result.reserve(starters.size());
	std::transform(begin(starters), end(starters), std::back_inserter(result),
		[](const StartingPositionDescription& spd)
		{
			RosterPosition r;
			r.name = std::string{ spd.name };
			r.offence = std::string{ spd.offence.position };
			r.defence = std::string{ spd.defence.position };
			r.offensive_score = spd.offence.score;
			r.defensive_score = spd.defence.score;
			r.total_score = spd.score;
			return r;
		});
	return result;
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
	{
		enum class ArgState
		{
			NotFound,
			Next,
			Found
		};
		ArgState td_state = ArgState::NotFound;
		ArgState cmp_state = ArgState::NotFound;
		for (int i = 1; i < argc; ++i)
		{
			std::string_view arg{ argv[i] };
			if (cicmp(arg, "--help") || cicmp(arg, "-help") || cicmp(arg, "help"))
			{
				std::cout << "Team Picker by arkadye.\n"
					"Usage: arguments optional.\n"
					"    --team-data [path]: a path to a team data file\n"
					"    --composition [path]: a path to a composition file\n"
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
		}
	}
	std::cout << "Loading " << team_data << '\n';
	std::ifstream team_input{ team_data };
	if (!team_input.is_open())
	{
		std::cout << "Could not open " << team_data << '\n';
		quit();
	}
	const std::vector<Player> roster = get_roster(team_input);

	std::cout << "Loading " << composition << '\n';
	std::ifstream req_input{ composition };
	if (!req_input.is_open())
	{
		std::cout << "Could not open " << composition << '\n';
		quit();
	}
	PositionRequirements requirements = parse_position_requirements(req_input);

	std::cout << "Picking the team...\n";
	std::vector<RosterPosition> picks = pick_team(roster, requirements);

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

	quit();
}
