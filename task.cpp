//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:54:46 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/src/isptech/coroutine/task.cpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#include "isptech/coroutine/task.hpp"
#include <cstdint>
#include <iostream>
#include <numeric>

#pragma warning(disable: 4073)
#pragma init_seg(lib)


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Coroutine {


void
print(const char* s)
{
    static std::mutex mtx;

    std::unique_lock<std::mutex> lock(mtx);
    std::cout << s << std::endl;
}


/*
    Names/Types
*/
using std::accumulate;
using std::count_if;
using std::find_if;
using std::iota;
using std::move;
using std::sort;
using std::try_to_lock;
using std::unique;
using std::upper_bound;


/*
    Data
*/
Scheduler scheduler;


/*
    Task Operation Selector Operation View
*/
inline
Task::Operation_selector::Operation_view::Operation_view(const Channel_operation* cop, Channel_size pos)
    : pop{cop}
    , index{pos}
{
}


inline Channel_base*
Task::Operation_selector::Operation_view::channel() const
{
    return pop->channel();
}


inline bool
Task::Operation_selector::Operation_view::dequeue(Task::Promise* taskp) const
{
    return pop->dequeue(taskp, index);
}


inline void
Task::Operation_selector::Operation_view::enqueue(Task::Promise* taskp) const
{
    pop->enqueue(taskp, index);
}


inline void
Task::Operation_selector::Operation_view::execute() const
{
    return pop->execute();
}


inline bool
Task::Operation_selector::Operation_view::is_ready() const
{
    return pop->is_ready();
}


inline Channel_size
Task::Operation_selector::Operation_view::position() const
{
    return index;
}


inline bool
Task::Operation_selector::Operation_view::operator==(Operation_view other) const
{
    return *this->pop == *other.pop;
}


inline bool
Task::Operation_selector::Operation_view::operator< (Operation_view other) const
{
    return *this->pop < *other.pop;
}


/*
    Task Operation Selector Channel Locks
*/
Task::Operation_selector::Channel_locks::Channel_locks(const Operation_vector& opers)
    : ops{opers}
{
    for_each_channel(ops, lock);
}


Task::Operation_selector::Channel_locks::~Channel_locks()
{
    for_each_channel(ops, unlock);
}


template<class T>
void
Task::Operation_selector::Channel_locks::for_each_channel(const Operation_vector& ops, T f)
{
    Channel_base* prevchanp{nullptr};

    for (const auto op : ops) {
        Channel_base* chanp = op.channel();
        if (chanp && chanp != prevchanp) {
            f(chanp);
            prevchanp = chanp;
        }
    }
}


inline void
Task::Operation_selector::Channel_locks::lock(Channel_base* chanp)
{
    chanp->lock();
}


inline void
Task::Operation_selector::Channel_locks::unlock(Channel_base* chanp)
{
    chanp->unlock();
}


/*
    Task Operation Selector Unique Operations
*/
Task::Operation_selector::Transform_unique::Transform_unique(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    transform(first, last, outp);
    sort(outp->begin(), outp->end());
    remove_duplicates(outp);
}


void
Task::Operation_selector::Transform_unique::remove_duplicates(Operation_vector* vp)
{
    const auto dup = unique(vp->begin(), vp->end());
    vp->erase(dup, vp->end());
}


void
Task::Operation_selector::Transform_unique::transform(const Channel_operation* first, const Channel_operation* last, Operation_vector* outp)
{
    Operation_vector& out = *outp;

    out.resize(last - first);
    for (const Channel_operation* cop = first; cop != last; ++cop) {
        const auto i = cop - first;
        out[i] = {cop, i};
    }
        
}


/*
    Task Operation Selector
*/
Channel_size
Task::Operation_selector::count_ready(const Operation_vector& ops)
{
    return count_if(ops.begin(), ops.end(), [](const auto op) {
        return op.is_ready();
    });
}


Channel_size
Task::Operation_selector::dequeue(Task::Promise* taskp, const Operation_vector& ops, Channel_size selected)
{
    Channel_size n = 0;

    for (const auto op : ops) {
        if (op.position() != selected && op.dequeue(taskp))
            ++n;
    }

    return n;
}


Channel_size
Task::Operation_selector::enqueue(Task::Promise* taskp, const Operation_vector& ops)
{
    for (const auto op : ops)
        op.enqueue(taskp);

    return ops.size();
}


Task::Select_status
Task::Operation_selector::notify_complete(Task::Promise* taskp, Channel_size pos)
{
    --nenqueued;
    if (!winner) {
        winner = pos;
        nenqueued -= dequeue(taskp, operations, pos);
    }

    return Select_status(*winner, nenqueued == 0);
}


Channel_size
Task::Operation_selector::pick_ready(const Operation_vector& ops, Channel_size nready)
{
    assert(ops.size() > 0 && nready > 0);

    Channel_size pos;
    Channel_size pick = random(1, nready);

    for (Channel_size i = 0, n = ops.size(); i < n; ++i) {
        if (ops[i].is_ready() && --pick == 0) {
            pos = i;
            break;
        }
    }

    return pos;
}


bool
Task::Operation_selector::select(Task::Promise* taskp, const Channel_operation* first, const Channel_operation* last)
{
    const Transform_unique  transform{first, last, &operations};
    const Channel_locks     lock{operations};

    nenqueued = 0;
    winner = select_ready(operations);
    if (!winner)
        nenqueued = enqueue(taskp, operations);

    return nenqueued == 0;
}


optional<Channel_size>
Task::Operation_selector::select_ready(const Operation_vector& ops)
{
    optional<Channel_size> ready;

    if (const auto n = count_ready(ops)) {
        const auto i    = pick_ready(ops, n);
        const auto op   = ops[i];

        op.execute();
        ready = op.position();
    }

    return ready;
}

   
Channel_size
Task::Operation_selector::selected() const
{
    return *winner;
}


optional<Channel_size>
Task::Operation_selector::try_select(const Channel_operation* first, const Channel_operation* last)
{
    const Transform_unique  transform{first, last, &operations};
    const Channel_locks     lock{operations};

    return select_ready(operations);
}


/*
    Task Future Selector Channel Locks
*/
void
Task::Future_selector::Channel_locks::acquire(const Future_wait_vector& fws, const Channel_wait_vector& cws, const Future_wait_index& index)
{
    transform(fws, cws, index, &channels);
    sort(&channels);
    lock(channels);
}


template<class T>
void
Task::Future_selector::Channel_locks::for_each_unique(const Channel_vector& cs, T func)
{
    Channel_base* prev = nullptr;

    for (auto pp = cs.begin(), end = cs.end(); pp != end; ++pp) {
        Channel_base* p = *pp;
        if (p && p != prev)
            func(p);
        prev = p;
    }
}


inline void
Task::Future_selector::Channel_locks::lock(const Channel_vector& chans)
{
    for_each_unique(chans, lock_channel);
}


inline void
Task::Future_selector::Channel_locks::lock_channel(Channel_base* chanp)
{
    chanp->lock();
}


inline
Task::Future_selector::Channel_locks::operator bool() const
{
    return !channels.empty();
}


void
Task::Future_selector::Channel_locks::release()
{
    unlock(channels);
    channels.clear();
}


inline void
Task::Future_selector::Channel_locks::sort(Channel_vector* chansp)
{
    std::sort(chansp->begin(), chansp->end());
}


void
Task::Future_selector::Channel_locks::transform(const Future_wait_vector& fws, const Channel_wait_vector& cws, const Future_wait_index& index, Channel_vector* chansp)
{
    chansp->reserve(cws.size());

    for (auto i : index) {
        const auto& fw = fws[i];
        chansp->push_back(cws[fw.value()].channel());
        chansp->push_back(cws[fw.error()].channel());
    }
}


inline void
Task::Future_selector::Channel_locks::unlock(const Channel_vector& chans)
{
    for_each_unique(chans, unlock_channel);
}


inline void
Task::Future_selector::Channel_locks::unlock_channel(Channel_base* chanp)
{
    chanp->unlock();
}


/*
    Task Future Selector Future Wait
*/
bool
Task::Future_selector::Future_wait::complete(Task::Promise* taskp, const Channel_wait_vector& waits, Channel_size pos) const
{
    *signalp = true;
    waits[pos].complete();
    return dequeue_unlocked(taskp, waits, other(pos, vpos, epos));
}


bool
Task::Future_selector::Future_wait::dequeue(Task::Promise* taskp, const Channel_wait_vector& waits) const
{
    const bool is_value_deq = waits[vpos].dequeue(taskp, vpos);
    const bool is_error_deq = waits[epos].dequeue(taskp, epos);

    return is_value_deq && is_error_deq;
}


bool
Task::Future_selector::Future_wait::dequeue(Task::Promise* taskp, const Channel_wait_vector& waits) const
{
    const auto& v       = waits[vpos];
    const auto& e       = waits[epos];
    const bool  v_was   = v.is_enqueued();
    const bool  e_was   = e.is_enqueued();

    if (v_was) v.dequeue(taskp, vpos);
    if (e_was) e.dequeue(taskp, epos);

    /*
        The task was dequeued from the future if it was enqueued on either
        channel but no longer is.
    */
    return (v_was || e_was) && !(v.is_enqueued() || e.is_enqueued());
}


inline bool
Task::Future_selector::Future_wait::dequeue_unlocked(Task::Promise* taskp, const Channel_wait_vector& waits) const
{
    const bool is_value_deq = dequeue_unlocked(taskp, waits, vpos);
    const bool is_error_deq = dequeue_unlocked(taskp, waits, epos);

    return is_value_deq && is_error_deq;
}


bool
Task::Future_selector::Future_wait::dequeue_unlocked(Task::Promise* taskp, const Channel_wait_vector& waits, Channel_size pos)
{
    bool        is_dequeued = true;
    const auto& wait = waits[pos];

    if (wait.is_enqueued()) {
        const Channel_lock lock{wait.channel()};
        is_dequeued = wait.dequeue(taskp, pos);
    }

    return is_dequeued;
}


inline Channel_size
Task::Future_selector::Future_wait::other(Channel_size pos, Channel_size pos1, Channel_size pos2)
{
    return pos == pos1 ? pos2 : pos1;
}


/*
    Task Future Selector Dequeue Locked
*/
inline
Task::Future_selector::Dequeue_locked::Dequeue_locked(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::Dequeue_locked::operator()(Channel_size n, Channel_size i) const
{
    if (futures[i].dequeue(taskp, channels))
        ++n;

    return n;
}


/*
    Task Future Selector Dequeue Unlocked
*/
inline
Task::Future_selector::dequeue_unlocked::dequeue_unlocked(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::dequeue_unlocked::operator()(Channel_size n, Channel_size i) const
{
    if (futures[i].dequeue_unlocked(taskp, channels))
        ++n;

    return n;
}


/*
    Task Future Selector Enqueue Not Ready
*/
inline
Task::Future_selector::Enqueue_not_ready::Enqueue_not_ready(Task::Promise* tp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
    : taskp{tp}
    , futures{fs}
    , channels{cs}
{
}


Channel_size
Task::Future_selector::Enqueue_not_ready::operator()(Channel_size n, Channel_size i) const
{
    const auto& f = futures[i];

    if (!f.is_ready(channels)) {
        f.enqueue(taskp, channels);
        ++n;
    }

    return n;
}


/*
    Task Future Selector Future Set
*/
Channel_size
Task::Future_selector::Future_set::count_ready(const Future_wait_index& index, const Future_wait_vector& fs, const Channel_wait_vector& cs)
{
    return count_if(index.begin(), index.end(), [&](auto i) {
        return fs[i].is_ready(cs);
    });
}


Channel_size
Task::Future_selector::Future_set::dequeue(Task::Promise* taskp)
{
    Channel_size n = 0;

    if (nenqueued > 0) {
        if (locks)
            n = accumulate(index.begin(), index.end(), 0, dequeue(taskp, futures, channels));
        else
            n = accumulate(index.begin(), index.end(), 0, dequeue_unlocked(taskp, futures, channels));

        nenqueued -= n;
    }

    return n;
}


inline Task::Future_selector::Dequeue_locked
Task::Future_selector::Future_set::dequeue(Task::Promise* taskp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
{
    return Dequeue_locked(taskp, fs, cs);
}


Channel_size
Task::Future_selector::Future_set::enqueue(Task::Promise* taskp)
{
    const auto n = accumulate(index.begin(), index.end(), 0, enqueue(taskp, futures, channels));

    nenqueued += n;
    return n;
}


inline Task::Future_selector::Enqueue_not_ready
Task::Future_selector::Future_set::enqueue(Task::Promise* taskp, const Future_wait_vector& fs, const Channel_wait_vector& cs)
{
    return Enqueue_not_ready(taskp, fs, cs);
}


void
Task::Future_selector::Future_set::index_unique(const Future_wait_vector& waits, Future_wait_index* indexp)
{
    init(indexp, waits);
    sort(indexp, waits);
    remove_duplicates(indexp, waits);
}


void
Task::Future_selector::Future_set::init(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    const auto n = waits.size();

    indexp->resize(n);
    iota(indexp->begin(), indexp->end(), 0);
}


void
Task::Future_selector::Future_set::lock_channels()
{
    locks.acquire(futures, channels, index);
}


Channel_size
Task::Future_selector::Future_set::notify_ready(Task::Promise* taskp, Channel_size chan)
{
    const Channel_size i = channels[chan].future();
    const Future_wait& f = futures[i];

    if (f.complete(taskp, channels, chan))
        --nenqueued;

    return i;
}


optional<Channel_size>
Task::Future_selector::Future_set::pick_ready(const Future_wait_index& index, const Future_wait_vector& futures, const Channel_wait_vector& chans, Channel_size nready)
{
    optional<Channel_size> ready;

    if (nready > 0) {
        Channel_size choice = random(1, nready);
        for (auto i : index) {
            if (futures[i].is_ready(chans) && --choice == 0) {
                ready = i;
                break;
            }
        }
    }

    return ready;
}


void
Task::Future_selector::Future_set::remove_duplicates(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    const auto dup = unique(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] == waits[y];
    });

    indexp->erase(dup, indexp->end());
}


optional<Channel_size>
Task::Future_selector::Future_set::select_ready()
{
    const auto n = count_ready(index, futures, channels);
    return pick_ready(index, futures, channels, n);
}


void
Task::Future_selector::Future_set::sort(Future_wait_index* indexp, const Future_wait_vector& waits)
{
    std::sort(indexp->begin(), indexp->end(), [&](auto x, auto y) {
        return waits[x] < waits[y];
    });
}


void
Task::Future_selector::Future_set::unlock_channels()
{
    locks.release();
}


/*
    Task Future Selector Timer
*/
inline void
Task::Future_selector::Timer::cancel(Task::Promise* taskp) const
{
    scheduler.cancel_timer(taskp);
    state = cancel_pending;
}


inline bool
Task::Future_selector::Timer::is_active() const
{
    return state != inactive;
}


inline bool
Task::Future_selector::Timer::is_cancelled() const
{
    return state == cancel_pending;
}


inline bool
Task::Future_selector::Timer::is_running() const
{
    return state == running;
}


inline void
Task::Future_selector::Timer::notify_cancelled() const
{
    state = inactive;
}


inline void
Task::Future_selector::Timer::notify_expired(Time_point /*when*/) const
{
    state = inactive;
}


/*
    Task Future Selector
*/
inline bool
Task::Future_selector::is_ready(const Future_set& futures, Timer timer)
{
    return !(futures.enqueued() || timer.is_active());
}


bool
Task::Future_selector::notify_channel_readable(Task::Promise* taskp, Channel_size chan)
{
    print("enter notify_channel_readable");

    const Channel_size f = futures.notify_ready(taskp, chan);

    if (quota > 0 && --quota == 0) {
        result = f;
        futures.dequeue(taskp);
        if (timer.is_running())
            timer.cancel(taskp);
    }

    if (is_ready(futures, timer))
        print("task is ready");
    else
        print("task still waiting");

    print("leaving notify_channel_readable");
    return is_ready(futures, timer);
}


bool
Task::Future_selector::notify_timer_expired(Task::Promise* taskp, Time_point time)
{
    print("enter notify_timer_expired");

    /*
        Timer expiration can race with timer cancellation, so ensure the
        timer hasn't already been cancelled before treating this as a
        timeout.
    */
    if (timer.is_cancelled()) {
        print("timer was cancelled");
        timer.notify_cancelled();
    }  else {
        print("timer has expired");
        timer.notify_expired(time);
        futures.dequeue(taskp);
        quota = 0;
    }

    if (!futures.enqueued())
        print("task is ready");
    else
        print("task still waiting");

    print("leaving notify_timer_expired");
    return !futures.enqueued();
}


bool
Task::Future_selector::notify_timer_cancelled()
{
    print("enter notify_timer_cancelled");
    timer.notify_cancelled();

    if (!futures.enqueued())
        print("task is ready");
    else
        print("task still waiting");

    print("leaving notify_timer_cancelled");
    return !futures.enqueued();
}


/*
    Task Promise
*/
Task::Promise::Promise()
    : taskstate{State::ready}
{
}


inline void
Task::Promise::make_ready()
{
    taskstate = State::ready;
}


inline Task::State
Task::Promise::state() const
{
    return taskstate;
}


inline void
Task::Promise::unlock()
{
    mutex.unlock();
}


/*
    Task
*/
Channel_size
Task::random(Channel_size min, Channel_size max)
{
    using Device        = std::random_device;
    using Engine        = std::default_random_engine;
    using Distribution  = std::uniform_int_distribution<Channel_size>;

    static Device   rand;
    static Engine   engine{rand()};
    Distribution    dist{min, max};

    return dist(engine);
}
    
   
/*
    Channel Operation
*/
Channel_operation::Channel_operation()
    : kind{Type::none}
    , chanp{nullptr}
    , rvalp{nullptr}
    , lvalp{nullptr}
{
}


Channel_operation::Channel_operation(Channel_base* cp, const void* rvaluep)
    : kind{Type::send}
    , chanp{cp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
{
    assert(cp != nullptr);
    assert(rvaluep != nullptr);
}


Channel_operation::Channel_operation(Channel_base* cp, void* lvaluep, Type optype)
    : kind{optype}
    , chanp{cp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
{
    assert(cp != nullptr);
    assert(lvaluep != nullptr);
}


inline Channel_base*
Channel_operation::channel() const
{
    return chanp;
}


bool
Channel_operation::dequeue(Task::Promise* taskp, Channel_size pos) const
{
    bool is_dequeued;

    if (chanp) {
        Task::Channel_lock lock(chanp);

        switch(kind) {
        case Type::send:
            is_dequeued = chanp->dequeue_write(taskp, pos);
            break;

        case Type::receive:
            is_dequeued = chanp->dequeue_read(taskp, pos);
            break;
        }
    }

    return is_dequeued;
}


void
Channel_operation::enqueue(Task::Promise* taskp, Channel_size pos) const
{
    if (chanp) {
        switch(kind) {
        case Type::send:
            if (rvalp)
                chanp->enqueue_write(taskp, pos, rvalp);
            else
                chanp->enqueue_write(taskp, pos, lvalp);
            break;

        case Type::receive:
            chanp->enqueue_read(taskp, pos, lvalp);
            break;
        }
    }
}


void
Channel_operation::execute() const
{
    if (chanp) {
        switch(kind) {
        case Type::send:
            if (rvalp)
                chanp->write(rvalp);
            else
                chanp->write(lvalp);
            break;

        case Type::receive:
            chanp->read(lvalp);
            break;
        }
    }
}


bool
Channel_operation::is_ready() const
{
    if (!chanp) return false;

    switch(kind) {
    case Type::send:    return chanp->is_writable();
    case Type::receive: return chanp->is_readable();
    default:            return false;
    }
}


/*
    Scheduler Suspended Tasks
*/
inline
Scheduler::Waiting_tasks::handle_eq::handle_eq(Task::Handle task)
    : h{task}
{
}


inline bool
Scheduler::Waiting_tasks::handle_eq::operator()(const Task& task) const
{
    return task.handle() == h;
}


void
Scheduler::Waiting_tasks::insert(Task&& task)
{
    const Lock lock{mutex};

    tasks.push_back(move(task));
    tasks.back().unlock();
}


Task
Scheduler::Waiting_tasks::release(Task::Handle h)
{
    Task        task;
    const Lock  lock{mutex};
    const auto  taskp = find_if(tasks.begin(), tasks.end(), handle_eq(h));

    if (taskp != tasks.end()) {
        task = move(*taskp);
        tasks.erase(taskp);
    }

    return task;
}


/*
    Sheduler Task Queue
*/
void
Scheduler::Task_queue::interrupt()
{
    const Lock lock{mutex};

    is_interrupt = true;
    ready.notify_one();
}


Task
Scheduler::Task_queue::pop()
{
    Task task;
    Lock lock{mutex};

    while (tasks.empty() && !is_interrupt)
        ready.wait(lock);

    if (!tasks.empty())
        task = pop_front(&tasks);

    return task;
}


inline Task
Scheduler::Task_queue::pop_front(std::deque<Task>* tasksp)
{
    Task task{move(tasksp->front())};

    tasksp->pop_front();
    return task;
}


void
Scheduler::Task_queue::push(Task&& task)
{
    const Lock lock{mutex};

    tasks.push_back(move(task));
    ready.notify_one();
}


Task
Scheduler::Task_queue::try_pop()
{
    Task        task;
    const Lock  lock{mutex, try_to_lock};

    if (lock && !tasks.empty())
        task = pop_front(&tasks);

    return task;
}


bool
Scheduler::Task_queue::try_push(Task&& task)
{
    bool        is_pushed{false};
    const Lock  lock{mutex, try_to_lock};

    if (lock) {
        tasks.push_back(move(task));
        ready.notify_one();
        is_pushed = true;
    }

    return is_pushed;
}


/*
    Scheduler Task Queue Array
*/
Scheduler::Task_queue_array::Task_queue_array(Size n)
    : qs{n}
{
}


void
Scheduler::Task_queue_array::interrupt()
{
    for (auto& q : qs)
        q.interrupt();
}


Task
Scheduler::Task_queue_array::pop(Size qpref)
{
    Task task;

    // Try to dequeue a task without waiting.
    for (Size i = 0, nqs = qs.size(); i < nqs && !task; ++i) {
        auto pos = (qpref + i) % nqs;
        task = qs[pos].try_pop();
    }

    // If that failed, wait on the preferred queue.
    if (!task)
        task = qs[qpref].pop();

    return task;
}


inline void
Scheduler::Task_queue_array::push(Task&& task)
{
    const auto nqs = qs.size();
    const auto qpref = nextq++ % nqs;

    push(&qs, qpref, move(task));
}


inline void
Scheduler::Task_queue_array::push(Size qpref, Task&& task)
{
    push(&qs, qpref, move(task));
}


void
Scheduler::Task_queue_array::push(Queue_vector* qvecp, Size qpref, Task&& task)
{
    Queue_vector&   qs  = *qvecp;
    const auto      nqs = qs.size();
    Size            i   = 0;

    // Try to enqueue the task without waiting.
    for (; i < nqs; ++i) {
        auto pos = (qpref + i) % nqs;
        if (qs[pos].try_push(move(task)))
            break;
    }

    // If that failed, wait on the preferred queue.
    if (i == nqs)
        qs[qpref].push(move(task));
}


inline Scheduler::Task_queue_array::Size
Scheduler::Task_queue_array::size() const
{
    return qs.size();
}


/*
    Scheduler Timers Request
*/
inline
Scheduler::Timers::Request::Request(Task::Promise* tp)
    : taskp{tp}
    , duration{}
{
}


inline
Scheduler::Timers::Request::Request(Task::Promise* tskp, nanoseconds t)
    : taskp{tskp}
    , duration{t}
{
}


inline bool
Scheduler::Timers::Request::is_cancel() const
{
    using namespace std::literals::chrono_literals;

    return duration == 0ns;
}


inline bool
Scheduler::Timers::Request::is_start() const
{
    return !is_cancel();
}


inline Task::Promise*
Scheduler::Timers::Request::task() const
{
    return taskp;
}


inline nanoseconds
Scheduler::Timers::Request::time() const
{
    return duration;
}


/*
    Scheduler Timers Alarm Queue
*/
inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::begin() const
{
    return alarms.begin();
}


inline Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::end() const
{
    return alarms.end();
}


inline void
Scheduler::Timers::Alarm_queue::erase(Iterator p)
{
    alarms.erase(p);
}


Scheduler::Timers::Alarm_queue::Iterator
Scheduler::Timers::Alarm_queue::find(Task::Promise* taskp) const
{
    const auto first    = alarms.begin();
    const auto last     = alarms.end();
    const auto p        = find_if(first, last, task_eq(taskp));

    return (p != last && p->taskp == taskp) ? p : last;
}


inline const Scheduler::Timers::Alarm&
Scheduler::Timers::Alarm_queue::front() const
{
    return alarms.front();
}


inline bool
Scheduler::Timers::Alarm_queue::is_empty() const
{
    return alarms.empty();
}


inline Scheduler::Timers::Alarm
Scheduler::Timers::Alarm_queue::pop()
{
    Alarm a = alarms.front();

    alarms.erase(alarms.begin());
    return a;
}


void
Scheduler::Timers::Alarm_queue::push(const Alarm& a)
{
    const auto p = upper_bound(alarms.begin(), alarms.end(), a);
    alarms.insert(p, a);
}


int
Scheduler::Timers::Alarm_queue::size() const
{
    return static_cast<int>(alarms.size());
}


/*
    Scheduler Timers Windows Handles
*/
Scheduler::Timers::Windows_handles::Windows_handles()
{
    int n = 0;

    /*
        TODO:  Implement error handling for Windows APIs (perhaps using
        std::system_error).
    */
    try {
        assert(request_handle == n);
        hs[n++] = CreateEvent(NULL, FALSE, FALSE, NULL);

        assert(timer_handle == n);
        hs[n++] = CreateWaitableTimer(NULL, FALSE, NULL);

        assert(interrupt_handle == n);
        hs[n++] = CreateEvent(NULL, FALSE, FALSE, NULL);

        assert(n == count);
    }
    catch (...) {
        close(hs, n);
    }
}


inline
Scheduler::Timers::Windows_handles::~Windows_handles()
{
    close(hs);
}


void
Scheduler::Timers::Windows_handles::close(Windows_handle* hs, int n)
{
    for (int i = 0; i < n; ++i)
        CloseHandle(hs[n - i - 1]);
}


inline void
Scheduler::Timers::Windows_handles::signal_interrupt() const
{
    SetEvent(hs[interrupt_handle]);
}


inline void
Scheduler::Timers::Windows_handles::signal_request() const
{
    SetEvent(hs[request_handle]);
}


inline Scheduler::Timers::Windows_handle
Scheduler::Timers::Windows_handles::timer() const
{
    return hs[timer_handle];
}


inline int
Scheduler::Timers::Windows_handles::wait_any(Lock* lockp) const 
{
    lockp->unlock();
    const DWORD n = WaitForMultipleObjects(count, hs, FALSE, INFINITE);
    lockp->lock();
    return static_cast<int>(n - WAIT_OBJECT_0);
}


/*
    Scheduler Timers
*/
Scheduler::Timers::Timers()
    : thread{[&]() { run_thread(); }}
{
}


Scheduler::Timers::~Timers()
{
    handles.signal_interrupt();
    thread.join();
}


void
Scheduler::Timers::activate_alarm(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    const Alarm alarm = make_alarm(request, now);

    queuep->push(alarm);
    if (alarm == queuep->front())
        set_timer(timer, alarm, now);
}


void
Scheduler::Timers::cancel(Task::Promise* taskp)
{
    const Lock lock{mutex}; 

    requestq.push(make_cancel(taskp));
    handles.signal_request();
}


void
Scheduler::Timers::cancel_alarm(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    const auto alarmp = queuep->find(request.task());

    if (alarmp != queuep->end()) {
        notify_cancel(*alarmp);
        remove_alarm(timer, queuep, alarmp, now);
    }
}


inline void
Scheduler::Timers::cancel_timer(Windows_handle handle)
{
    CancelWaitableTimer(handle);
}


inline bool
Scheduler::Timers::is_expired(const Alarm& alarm, Time_point now)
{
    return alarm.expiry <= now;
}


inline Scheduler::Timers::Alarm
Scheduler::Timers::make_alarm(const Request& request, Time_point now)
{
    return Alarm(request.task(), now + request.time());
}


inline Scheduler::Timers::Request
Scheduler::Timers::make_cancel(Task::Promise* taskp)
{
    return Request(taskp);
}


inline Scheduler::Timers::Request
Scheduler::Timers::make_start(Task::Promise* taskp, nanoseconds duration)
{
    return Request(taskp, duration);
}


inline void
Scheduler::Timers::notify_cancel(const Alarm& alarm)
{
    Task::Promise* taskp = alarm.taskp;

    if (taskp->notify_timer_cancelled())
        scheduler.resume(taskp);
}


void
Scheduler::Timers::notify_complete(const Alarm& alarm, Time_point now, Lock* lockp)
{
    Task::Promise* taskp = alarm.taskp;

    /*
        If the waiting task has already been awakened, it could be in the
        midst of cancelling the timer.  To avoid a deadly embrace, the current
        thread unlocks the timers while notifying the waiter that the timer
        has completed.
    */
    lockp->unlock();
    if (taskp->notify_timer_expired(now))
        scheduler.resume(taskp);
    lockp->lock();
}


void
Scheduler::Timers::process_alarms(Windows_handle timer, Alarm_queue* queuep, Lock* lockp)
{
    const Time_point now = Clock::now();

    while (!queuep->is_empty() && is_expired(queuep->front(), now))
        notify_complete(queuep->pop(), now, lockp);

    if (!queuep->is_empty())
        set_timer(timer, queuep->front(), now);
}


void
Scheduler::Timers::process_request(Windows_handle timer, const Request& request, Alarm_queue* queuep, Time_point now)
{
    if (request.is_start())
        activate_alarm(timer, request, queuep, now);
    else if (request.is_cancel())
        cancel_alarm(timer, request, queuep, now);
}


void
Scheduler::Timers::process_requests(Windows_handle timer, Request_queue* requestqp, Alarm_queue* queuep)
{
    const Time_point now = Clock::now();

    while (!requestqp->empty()) {
        process_request(timer, requestqp->front(), queuep, now);
        requestqp->pop();
    }
}


void
Scheduler::Timers::remove_alarm(Windows_handle timer, Alarm_queue* queuep, Alarm_queue::Iterator p, Time_point now)
{
    // If the alarm is the next to fire, update the timer.
    if (p == queuep->begin()) {
        const auto nextp = std::next(p);
        if (nextp == queuep->end())
            cancel_timer(timer);
        else if (*p < *nextp)
            set_timer(timer, *nextp, now);
    }

    queuep->erase(p);
}


void
Scheduler::Timers::run_thread()
{
    bool done{false};
    Lock lock{mutex};

    while (!done) {
        switch(handles.wait_any(&lock)) {
        case request_handle:
            process_requests(handles.timer(), &requestq, &alarmq);
            break;

        case timer_handle:
            process_alarms(handles.timer(), &alarmq, &lock);
            break;

        default:
            done = true;
            break;
        }
    }
}


void
Scheduler::Timers::set_timer(Windows_handle timer, const Alarm& alarm, Time_point now)
{
    static const int nanosecs_per_tick = 100;

    const nanoseconds   dt      =  alarm.expiry - now;
    const std::int64_t  timerdt = -(dt.count() / nanosecs_per_tick);
    LARGE_INTEGER       timebuf;

    timebuf.LowPart = static_cast<DWORD>(timerdt & 0xFFFFFFFF);
    timebuf.HighPart = static_cast<LONG>(timerdt >> 32);
    SetWaitableTimer(timer, &timebuf, 0, NULL, NULL, FALSE);
}


void
Scheduler::Timers::start(Task::Promise* taskp, nanoseconds duration)
{
    const Lock lock{mutex};

    requestq.push(make_start(taskp, duration));
    handles.signal_request();
}


/*
    Scheduler
*/
Scheduler::Scheduler(int nthreads)
    : ready{nthreads > 0 ? nthreads : Thread::hardware_concurrency()}
{
    const auto nqs = ready.size();

    processors.reserve(nqs);
    for (unsigned q = 0; q != nqs; ++q)
        processors.emplace_back([&,q]{ run_tasks(q); });
}


/*
    TODO:  Could this destructor should be rendered unnecessary by
    arranging for (a) workqueues to shutdown implicitly (in their
    destructors) and (b) workqueues to be joined implicitly (in their
    destructors)?
*/
Scheduler::~Scheduler()
{
    ready.interrupt();
    for (auto& proc : processors)
        proc.join();
}


void
Scheduler::cancel_timer(Task::Promise* taskp)
{
    timers.cancel(taskp);
}


void
Scheduler::resume(Task::Promise* taskp)
{
    auto task = Task::Handle::from_promise(*taskp);
    ready.push(waiting.release(task));
}


void
Scheduler::run_tasks(unsigned q)
{
    while (Task task = ready.pop(q)) {
        try {
            switch(task.resume()) {
            case Task::State::ready:
                ready.push(q, move(task));
                break;

            case Task::State::waiting:
                waiting.insert(move(task));
                break;
            }
        } catch (...) {
            ready.interrupt();
        }
    }
}


void
Scheduler::start_timer(Task::Promise* taskp, nanoseconds duration)
{
    timers.start(taskp, duration);
}


void
Scheduler::submit(Task task)
{
    ready.push(move(task));
}


}   // Coroutine
}   // Isptech

//  $CUSTOM_FOOTER$
