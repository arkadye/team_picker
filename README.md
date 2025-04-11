# Team Picker
Input team stats (you may need to adjust the input parser) and get an ideal team output. Handles offence/defence positions separately. Includes default picker for BrutalBall.

## To use:

### Released binary

Download from here: https://github.com/arkadye/team_picker/releases

### Building with CMake

If you do not already have CMake set up, setup CMake.

There are is no special setup. Simply run CMake on the folder containing `CMakeLists.txt`. (e.g. `cmake .` from the folder containing it) or `cmake . --build`.

A CMake tutorial is beyond the scope of this Readme.

### Building from source

Get yourself a C++ compiler. This was written with Visual Studio Community Edition 2022 so I know for sure that works.

Compile main.cpp with C++20 or later enabled.

Setup your composition.txt to describe the positions being picked and the calculations to base each position on.

Put your team, with their stats, into team_data.txt.

Make sure team_data.txt and composition.txt are in the current working directory.

Compile and run main.cpp.

The team to pick will be output to stdout.

## Inputs

When run with no command line arguments the program will load team data from `team_data.txt` and composition data from `composition.txt`, both in the current working directly. This is typically the same folder as the program.

From the command line it is possible to specify specific files using the command line.

The command line options are:

- `--help`: if this is set anywhere it will print out some help text and then exit.
- `--team-data`: the next argument is a the path to team data.
- `--composition`: the next argument is the path to a composition file.

These can be put in any order, or omitted entirely. Valid ways to invoke from the command line include: `team_picker.exe` (with no args); `team_picker.exe --team-data sunday_league/main_squad.txt`; `team_picker.exe --composition high_offence.txt`; `team_picker.exe --team-data custom_draft.txt --composition ../high_defence.txt`; or even `team_picker.exr --composition pick_methods/all_out_attack.txt --team-data.txt thursday_league.txt`.

## Team data file

By default this is `team_data.txt`.

The parser converts this format:

    Name Stat1 Stat2
    #00 Joe Bloggs
    Metadata about him [which may be in brackets] 111 222
    #05 Jane Doe
    Other data 333 444
    
and so on. The first line gives a list of stats to collect. Each player is then two lines: the first is a shirt number (ignored) and a name (assigned to `name`). The second line is metadata which is ignored - it will be parsed until it finds  a number - and then the values are assigned to the stats. So `Joe Bloggs` ends up with `Stat1=111` `Stat2=222` and `Jane Doe` ends up with `Stat1=333` `Stat2=444`. The name and number of stats can be set arbitrarily.

If you want a different format you'll have to edit/rewrite the `get_player()` function  and maybe the `read_header()` function too. 

## Draft Evaluation data file

By default this is `draft_class.txt`, but only if that file actually exists.

If a draft class file is set, after picking a team this will iterate every player in the draft class and try to put them in the team. It will note how much they improve/hurt the team by, and their best position.

It will then output a list of all the draftees, from best to worst.

## Composition

By default this is `composition.txt`. Alternatives can be set from the command line.

Any line beginning with `#` is a comment and will be ignored.

List offensive positions on a line starting with `Offence:` and defensive ones on a line starting `Defence:`. Both lines must contain an equal number of positions.

Positions can be repeated. An association football team would like like `GK LB CB CB RB LM CM CM RW ST ST` for example.

Force a player to be on the team with `Force:` and then the player name. So `Force: Joe Bloggs` will force a player named *exactly* "Joe Bloggs" to be on the team, even if their evaluation is otherwise awful.

Set calculations based on stats.

If a goalkeeper has stats `Sav` and `Pos` you could set the goalkeeper's score with `GK = Sav + Pos` for example. The name of a stat can be used as a variable name and will be substituted in for the value of the stat associated with that player when evaluating that player in that position.

All names of positions, stats, and functions are NOT case sensitive. `sav`, `SAV`, `sAv`, etc... are all equivalent to each other.

The following mathematical operations are supported:

`+`; `-`; `*`; `/`; `(brackets)`; `^` (for exponents); `min` (with any number of arguments); `max` (with any number of arguments),;`pow` (with two arguments: `pow(a,b)` is equivalent to `a^b`); `average` with any number of arguments.

Values can be converted to `bool`s. Any value `abs(val)<0.5` is considered `false`, everything else is `true`. Conversely `false` converts to `0.0` and `true` converts to `1.0`.

The following boolean operations are supported: `&&` (and), `||` (or), `not`, `if(bool,true_expression,false_expression)`, `<`, `>`, `<=`, `>=`, `==` and `!=`.

## Method

First the `team_data.txt` file is converted to a vector of `Player` objects. Players are evaluated at each position, and their best offence and defence positions cached.

The team is sorted based on the total of best offence and best defence positions. The best players by this sort are set as starters and every permutation of offensive and defensive arrangements of those players are tried to get the offensive and defensive positions for that line up.

A team is scored by adding the scores of each player in their offensive and defensive positions.

Then the entire roster is iterated through looking for a player who is not in the line up. Then it will try swapping them in for each player, taking their offensive position. It will then find the best defensive line up again. The new line up is compared to the old one. If it scores better than the old one this setup replaces the old one and the process is restarted from the beginning of this paragraph. This repeats until no substitution improves the team.
