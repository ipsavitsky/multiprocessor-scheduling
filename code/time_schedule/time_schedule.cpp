#include "time_schedule.hpp"

#include <algorithm>

/**
 * @brief Construct a new Time Schedule:: Time Schedule object
 * 
 * @param proc_num Amount of processors in the schedule
 */
TimeSchedule::TimeSchedule(std::size_t proc_num) {
    proc_array.resize(proc_num);
}

/**
 * @brief Calculate the time it takes to execute the schedule
 * 
 * @return int 
 */
int TimeSchedule::get_time() const {
    return std::max_element(proc_array.begin(), proc_array.end(),
                            [](const proc_info &a, const proc_info &b) {
                                return a.back().finish < b.back().finish;
                            })
        ->back()
        .finish;
}

/**
 * @brief Add a task to the schedule
 *
 * @todo check if test_add_task is needed
 * 
 * @param sched Schedule with all dependencies
 * @param task The task to add
 * @param proc Processor to assign to the task
 */
void TimeSchedule::add_task(const Schedule &sched, const Schedule::Task &task,
                            const Schedule::Proc &proc) {
    auto x = test_add_task(sched, task, proc);
    PlacedTask placed_task{task, x, x + sched.get_task_time(proc, task)};
    proc_array[proc].push_back(placed_task);
    fast_mapping[task] = proc;
    for (auto it = sched.get_in_edges(task); it.first != it.second;
         ++it.first) {
        auto from = boost::source(*it.first, sched.get_graph());
        if (fast_mapping[from] != proc) {
            ++amount_of_transitions;
            if (!sched.is_direct_connection(fast_mapping[from], proc))
                ++amount_of_indirect_transitions;
        }
    }
}

/**
 * @brief Test if a task can be added to the schedule
 * 
 * @param sched Schedule with all dependencies
 * @param task Task to add
 * @param proc Processor to assign to the task
 * @return int
 */
std::size_t TimeSchedule::test_add_task(const Schedule &sched,
                                        const Schedule::Task &task,
                                        const Schedule::Proc &proc) {
    std::vector<int> times;
    for (auto it = sched.get_in_edges(task); it.first != it.second;
         ++it.first) {
        auto from = boost::source(*it.first, sched.get_graph());
        // debug output commented because it generates way to much output
        // LOG_DEBUG << "from: " << from << " to: " << task;
        int trans_time = sched.get_task_time(proc, task);
        int task_time = sched.get_tran_time(fast_mapping[from], proc);
        // LOG_DEBUG << "trans_time: " << trans_time
        //           << "; tran_time: " << task_time;
        times.push_back(trans_time + task_time);
    }
    auto first_available_dependencies =
        *std::max_element(times.begin(), times.end());

    auto stuff = proc_array[proc];
    int first_available_processor;
    if (stuff.begin() != stuff.end()) {
        first_available_processor =
            std::max_element(proc_array[proc].begin(), proc_array[proc].end(),
                             [](const PlacedTask &a, const PlacedTask &b) {
                                 return a.finish < b.finish;
                             })
                ->finish;
    } else {
        first_available_processor = 0;
    }
    return std::max(first_available_dependencies, first_available_processor);
}

/**
 * @brief Calculate the GC2 score of the schedule
 * 
 * @param sched Schedule with all dependencies
 * @param task 
 * @return Schedule::Proc 
 */
Schedule::Proc TimeSchedule::GC2(const Schedule &sched, Schedule::Task task) {
    std::vector<std::pair<Schedule::Proc, int>> times;
    for (int i = 0; i < proc_array.size(); i++) {
        times.push_back({i, test_add_task(sched, task, i)});
    }
    auto best_proc = std::min_element(
        times.begin(), times.end(),
        [](const auto &a, const auto &b) { return a.second < b.second; });
    return best_proc->first;
}

/**
 * @brief Calculate \f$crit_{CR} \f$
 * 
 * \f$crit_{CR} = C_1 \cdot GC_2 + C_2 \cdot CR + C_3 \cdot CR_2\f$
 * 
 * @param sched Schedule with all dependencies
 * @param task Task to find the placing of
 * @param C1, C2, C3 Coefficients 
 * @return Schedule::Proc 
 */
Schedule::Proc TimeSchedule::GC2_CR(const Schedule &sched, Schedule::Task task,
                                    double C1, double C2, double C3) {
    std::vector<std::pair<Schedule::Proc, int>> times;
    for (int i = 0; i < proc_array.size(); i++) {
        LOG_DEBUG << C1 * test_add_task(sched, task, i) << " "
                  << C2 * CR_with_task(sched, task, i);
        times.push_back({i, C1 * test_add_task(sched, task, i) +
                                C2 * CR_with_task(sched, task, i)});
    }
    return std::min_element(
               times.begin(), times.end(),
               [](const auto &a, const auto &b) { return a.second < b.second; })
        ->first;
}

/**
 * @brief Calculate \f$crit_{BF} \f$
 * 
 * \f$crit_{CR} = C_1 \cdot GC_2 + C_2 \cdot BF \f$
 * 
 * @param sched Schedule with all dependencies
 * @param task Task to find the placing of
 * @param C1, C2 Coefficients
 * @return Schedule::Proc 
 */
Schedule::Proc TimeSchedule::GC2_BF(const Schedule &sched, Schedule::Task task,
                                    double C1, double C2) {
    std::vector<std::pair<Schedule::Proc, int>> times;
    for (int i = 0; i < proc_array.size(); i++) {
        times.push_back({i, C1 * test_add_task(sched, task, i) +
                                C2 * BF_with_task(sched, task, i)});
    }
    return std::min_element(
               times.begin(), times.end(),
               [](const auto &a, const auto &b) { return a.second < b.second; })
        ->first;
}

double TimeSchedule::CR_with_task(const Schedule &sched, Schedule::Task task,
                                  Schedule::Proc proc) const {
    TimeSchedule copy(*this);
    copy.add_task(sched, task, proc);
    return copy.calculate_CR(sched);
}

double TimeSchedule::BF_with_task(const Schedule &sched, Schedule::Task task,
                                  Schedule::Proc proc) const {
    TimeSchedule copy(*this);
    copy.add_task(sched, task, proc);
    return copy.calculate_BF();
}

/**
 * @brief Calculate `BF` of current schedule
 * 
 * @return double 
 */
double TimeSchedule::calculate_BF() const {
    auto max_tasks =
        std::max_element(proc_array.begin(), proc_array.end(),
                         [](const proc_info &a, const proc_info &b) {
                             return a.size() < b.size();
                         })
            ->size();
    size_t amount_of_tasks = 0;
    for (auto &proc : proc_array) {
        amount_of_tasks += proc.size();
    }
    double BF = 100 * ((max_tasks * proc_array.size() / amount_of_tasks) - 1);
    return std::ceil(BF);
}


/**
 * @brief Calculate `CR` of current schedule
 * 
 * @param sched Schedule with all dependencies
 * @return double 
 */
double TimeSchedule::calculate_CR(const Schedule &sched) const {
    LOG_DEBUG << amount_of_transitions << " "
              << boost::num_edges(sched.get_graph());
    return amount_of_transitions / boost::num_edges(sched.get_graph());
}

/**
 * @brief Calculate `CR2` of current schedule
 * 
 * @param sched Schedule with all dependencies
 * @return double 
 */
double TimeSchedule::calculate_CR2(const Schedule &sched) const {
    return amount_of_indirect_transitions / boost::num_edges(sched.get_graph());
}