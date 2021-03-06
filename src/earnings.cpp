//=======================================================================
// Copyright (c) 2013-2018 Baptiste Wicht.
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <iostream>
#include <fstream>
#include <sstream>

#include "earnings.hpp"
#include "args.hpp"
#include "accounts.hpp"
#include "data.hpp"
#include "guid.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "console.hpp"
#include "writer.hpp"
#include "budget_exception.hpp"

using namespace budget;

namespace {

static data_handler<earning> earnings { "earnings", "earnings.data" };

} //end of anonymous namespace

std::map<std::string, std::string> budget::earning::get_params(){
    std::map<std::string, std::string> params;

    params["input_id"]      = budget::to_string(id);
    params["input_guid"]    = guid;
    params["input_date"]    = budget::to_string(date);
    params["input_name"]    = name;
    params["input_account"] = budget::to_string(account);
    params["input_amount"]  = budget::to_string(amount);

    return params;
}

void budget::earnings_module::load(){
    load_earnings();
    load_accounts();
}

void budget::earnings_module::unload(){
    save_earnings();
}

void budget::earnings_module::handle(const std::vector<std::string>& args){
    console_writer w(std::cout);

    if(args.size() == 1){
        show_earnings(w);
    } else {
        auto& subcommand = args[1];

        if(subcommand == "show"){
            if(args.size() == 2){
                show_earnings(w);
            } else if(args.size() == 3){
                show_earnings(budget::month(to_number<unsigned short>(args[2])),
                    w);
            } else if(args.size() == 4){
                show_earnings(
                    budget::month(to_number<unsigned short>(args[2])),
                    budget::year(to_number<unsigned short>(args[3])),
                    w);
            } else {
                throw budget_exception("Too many arguments to earning show");
            }
        } else if(subcommand == "all"){
            show_all_earnings(w);
        } else if(subcommand == "add"){
            earning earning;
            earning.guid = generate_guid();
            earning.date = budget::local_day();

            edit_date(earning.date, "Date");

            std::string account_name;
            edit_string_complete(account_name, "Account", all_account_names(), not_empty_checker(), account_checker(earning.date));
            earning.account = get_account(account_name, earning.date.year(), earning.date.month()).id;

            edit_string(earning.name, "Name", not_empty_checker());
            edit_money(earning.amount, "Amount", not_negative_checker());

            auto id = earnings.add(std::move(earning));
            std::cout << "earning " << id << " has been created" << std::endl;
        } else if(subcommand == "delete"){
            enough_args(args, 3);

            size_t id = to_number<size_t>(args[2]);

            if(!earnings.exists(id)){
                throw budget_exception("There are no earning with id ");
            }

            earnings.remove(id);

            std::cout << "earning " << id << " has been deleted" << std::endl;
        } else if(subcommand == "edit"){
            enough_args(args, 3);

            size_t id = to_number<size_t>(args[2]);

            if(!earnings.exists(id)){
                throw budget_exception("There are no earning with id " + args[2]);
            }

            auto& earning = earnings[id];

            edit_date(earning.date, "Date");

            auto account_name = get_account(earning.account).name;
            edit_string_complete(account_name, "Account", all_account_names(), not_empty_checker(), account_checker(earning.date));
            earning.account = get_account(account_name, earning.date.year(), earning.date.month()).id;

            edit_string(earning.name, "Name", not_empty_checker());
            edit_money(earning.amount, "Amount", not_negative_checker());

            if (earnings.edit(earning)) {
                std::cout << "Earning " << id << " has been modified" << std::endl;
            }
        } else {
            throw budget_exception("Invalid subcommand \"" + subcommand + "\"");
        }
    }
}

void budget::load_earnings(){
    earnings.load();
}

void budget::save_earnings(){
    earnings.save();
}

std::ostream& budget::operator<<(std::ostream& stream, const earning& earning){
    return stream << earning.id  << ':' << earning.guid << ':' << earning.account << ':' << earning.name << ':' << earning.amount << ':' << to_string(earning.date);
}

void budget::operator>>(const std::vector<std::string>& parts, earning& earning){
    bool random = config_contains("random");

    earning.id = to_number<size_t>(parts[0]);
    earning.guid = parts[1];
    earning.account = to_number<size_t>(parts[2]);
    earning.name = parts[3];
    earning.date = from_string(parts[5]);

    if(random){
        earning.amount = budget::random_money(10, 5000);
    } else {
        earning.amount = parse_money(parts[4]);
    }
}

std::vector<earning>& budget::all_earnings(){
    return earnings.data;
}

void budget::set_earnings_changed(){
    earnings.set_changed();
}

void budget::set_earnings_next_id(size_t next_id){
    earnings.next_id = next_id;
}

void budget::add_earning(budget::earning&& earning){
    earnings.add(std::forward<budget::earning>(earning));
}

void budget::show_all_earnings(budget::writer& w){
    w << title_begin << "All Earnings " << add_button("earnings") << title_end;

    std::vector<std::string> columns = {"ID", "Date", "Account", "Name", "Amount"};
    std::vector<std::vector<std::string>> contents;

    for(auto& earning : earnings.data){
        contents.push_back({to_string(earning.id), to_string(earning.date), get_account(earning.account).name, earning.name, to_string(earning.amount)});
    }

    w.display_table(columns, contents);
}

void budget::search_earnings(const std::string& search, budget::writer& w){
    w << title_begin << "Results" << title_end;

    std::vector<std::string> columns = {"ID", "Date", "Account", "Name", "Amount", "Edit"};
    std::vector<std::vector<std::string>> contents;

    money total;
    size_t count = 0;

    auto l_search = search;
    std::transform(l_search.begin(), l_search.end(), l_search.begin(), ::tolower);

    for(auto& earning : earnings.data){
        auto l_name = earning.name;
        std::transform(l_name.begin(), l_name.end(), l_name.begin(), ::tolower);

        if(l_name.find(l_search) != std::string::npos){
            contents.push_back({to_string(earning.id), to_string(earning.date), get_account(earning.account).name, earning.name, to_string(earning.amount), "::edit::earnings::" + to_string(earning.id)});

            total += earning.amount;
            ++count;
        }
    }

    if(count == 0){
        w << "No earnings found" << end_of_line;
    } else {
        contents.push_back({"", "", "", "Total", to_string(total), ""});

        w.display_table(columns, contents, 1, {}, 0, 1);
    }
}

void budget::show_earnings(budget::month month, budget::year year, budget::writer& w){
    w << title_begin << "Earnings of " << month << " " << year << " "
      << add_button("earnings")
      << budget::year_month_selector{"earnings", year, month} << title_end;

    std::vector<std::string> columns = {"ID", "Date", "Account", "Name", "Amount", "Edit"};
    std::vector<std::vector<std::string>> contents;

    money total;
    size_t count = 0;

    for(auto& earning : earnings.data){
        if(earning.date.year() == year && earning.date.month() == month){
            contents.push_back({to_string(earning.id), to_string(earning.date), get_account(earning.account).name, earning.name, to_string(earning.amount), "::edit::earnings::" + to_string(earning.id)});

            total += earning.amount;
            ++count;
        }
    }

    if(count == 0){
        w << "No earnings for " << month << "-" << year << end_of_line;
    } else {
        contents.push_back({"", "", "", "Total", to_string(total), ""});

        w.display_table(columns, contents, 1, {}, 0, 1);
    }
}

void budget::show_earnings(budget::month month, budget::writer& w){
    auto today = budget::local_day();

    budget::show_earnings(month, today.year(), w);
}

void budget::show_earnings(budget::writer& w){
    auto today = budget::local_day();

    budget::show_earnings(today.month(), today.year(), w);
}

bool budget::earning_exists(size_t id){
    return earnings.exists(id);
}

void budget::earning_delete(size_t id) {
    if (!earnings.exists(id)) {
        throw budget_exception("There are no earning with id ");
    }

    earnings.remove(id);
}

earning& budget::earning_get(size_t id) {
    if (!earnings.exists(id)) {
        throw budget_exception("There are no earning with id ");
    }

    return earnings[id];
}
