#include "logging/boost_logger.hpp"
#include "parser/huawei_parser.hpp"
#include "schedule/schedule.hpp"
#include "time_schedule/time_schedule.hpp"

#include <boost/program_options.hpp>

#include <fstream>

int main(int argc, char *argv[]) {

    boost::program_options::options_description desc("General options");
    std::string str_criteria;
    std::string filename;
    desc.add_options()("help,h", "Print help")(
        "input,i",
        boost::program_options::value<std::string>(&filename)->default_value(
            "../input.txt"),
        "Input file")("criteria,c",
                      boost::program_options::value<std::string>(&str_criteria)
                          ->default_value("NO"),
                      "Extra criteria for time schedule");
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    logger::debug = true;
    logger::init();

    LOG_INFO << "Starting";

    TimeSchedule::extra_criteria criteria;
    if (str_criteria == "NO") {
        criteria = TimeSchedule::extra_criteria::NO;
    } else if (str_criteria == "BF") {
        criteria = TimeSchedule::extra_criteria::BF;
    } else if (str_criteria == "CR") {
        criteria = TimeSchedule::extra_criteria::CR;
    } else {
        LOG_ERROR << "Unknown criteria: " << str_criteria;
        return 1;
    }

    Schedule schedule = new_schedule(filename);

    TimeSchedule time_schedule(schedule.get_proc_num());

    auto D = schedule.get_top_vertices();

    LOG_INFO << "D updated";

    schedule.create_fictive_node(D);

    LOG_INFO << "Fictive node created";

    schedule.set_up_critical_paths();

    LOG_INFO << "Calculated critical paths";

    schedule.hard_remove_fictive_vertices();

    while (!D.empty()) {
        auto chosen_task = schedule.GC1(D);
        LOG_INFO << "GC1 chosen " << chosen_task;
        Schedule::Proc chosen_proc;
        switch (criteria) {
        case TimeSchedule::extra_criteria::NO:
            chosen_proc = time_schedule.GC2(schedule, chosen_task);
            break;
        case TimeSchedule::extra_criteria::CR:
            chosen_proc =
                time_schedule.GC2_CR(schedule, chosen_task, 1, 1, 0.5);
            break;
        case TimeSchedule::extra_criteria::BF:
            chosen_proc = time_schedule.GC2_BF(schedule, chosen_task, 1, 0.7);
            break;
        }
        LOG_INFO << "GC2 chosen " << chosen_proc;
        time_schedule.add_task(schedule, chosen_task, chosen_proc);
        // time_schedule.test_add_task(schedule, 4, 2);
        schedule.remove_vertex(chosen_task);
        D = schedule.get_top_vertices();
    }
    LOG_INFO << "time:\t" << time_schedule.get_time();
    return 0;
}