//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/channel.cpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/channel.cpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


#include "isptech/concurrency/channel.hpp"
#include <algorithm>

#pragma warning(disable: 4073)
#pragma init_seg(lib)

/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


// Names/Types
Scheduler scheduler;


/*
    Names/Types
*/
using std::get;
using std::find_if;
using std::move;
using std::tie;
using std::try_to_lock;


/*
    Implementation Details
*/
namespace Detail {
    
    
/*
    Sheduler Work Queue
*/
void
Workqueue::interrupt()
{
    Lock lock{mutex};

    is_interrupt = true;
    lock.unlock();
    ready.notify_all();
}


tuple<Goroutine,bool>
Workqueue::pop()
{
    tuple<Goroutine,bool>   result;
    Lock                    lock{mutex};

    while (q.is_empty() && !is_interrupt)
        ready.wait(lock);

    if (!q.is_empty()) {
        get<0>(result) = q.pop();
        get<1>(result) = true;
    }

    return result;
}


void
Workqueue::push(Goroutine&& g)
{
    Lock lock{mutex};

    q.push(move(g));
    lock.unlock();
    ready.notify_one();
}


tuple<Goroutine,bool>
Workqueue::try_pop()
{
    tuple<Goroutine,bool>   result;
    Lock                    lock{mutex, try_to_lock};

    if (lock && !q.is_empty()) {
        get<0>(result) = q.pop();
        get<1>(result) = true;
    }

    return result;
}


bool
Workqueue::try_push(Goroutine&& g)
{
    bool is_done{false};
    Lock lock{mutex, try_to_lock};

    if (lock) {
        q.push(move(g));
        lock.unlock();
        ready.notify_one();
        is_done = true;
    }

    return is_done;
}


/*
    Work Queue Goroutine Queue
*/
inline bool
Workqueue::Goroutine_queue::is_empty() const
{
    return elems.empty();
}


inline Goroutine
Workqueue::Goroutine_queue::pop()
{
    Goroutine g{move(elems.front())};

    elems.pop_front();
    return g;
}


inline void
Workqueue::Goroutine_queue::push(Goroutine&& g)
{
    elems.push_back(move(g));
}


/*
    Scheduler Work Queue Array
*/
Workqueue_array::Workqueue_array(Size n)
    : queues{n}
{
}


void
Workqueue_array::interrupt()
{
    for (auto& q : queues)
        q.interrupt();
}


tuple<Goroutine,bool>
Workqueue_array::pop(Size threadq)
{
    const auto              nqueues = queues.size();
    tuple<Goroutine,bool>   result;

    // try to obtain work without blocking (preferably from this thread's queue)
    for (Size i = 0; !get<1>(result) && i != nqueues; ++i) {
        auto index = (threadq + i) % nqueues;
        result = queues[index].try_pop();
    }

    // if we failed, wait on this thread's queue
    if (!get<1>(result))
        result = queues[threadq].pop();

    return result;
}


void
Workqueue_array::push(Goroutine&& g)
{
    auto q = nextqueue++ % size();
    push(q, move(g));
}


void
Workqueue_array::push(Size threadq, Goroutine&& g)
{
    const auto  nqueues     = queues.size();
    bool        is_enqueued = false;

    // try to enqueue the work without blocking (preferably on this thread's queue)
    for (Size i = 0; !is_enqueued && i != nqueues; ++i) {
        auto index = (threadq + i) % nqueues;
        if (queues[index].try_push(move(g)))
            is_enqueued = true;
     }

    // if we failed, wait on this thread's queue 
    if (!is_enqueued)
        queues[threadq].push(move(g));
}


inline Workqueue_array::Size
Workqueue_array::size() const
{
    return queues.size();
}


/*
    Goroutine List
*/
inline
Goroutine_list::handle_equal::handle_equal(Goroutine::Handle gh)
    : h(gh)
{
}


inline bool
Goroutine_list::handle_equal::operator()(const Goroutine& g) const
{
    return g.handle() == h;
}


void
Goroutine_list::insert(Goroutine&& g)
{
    Lock lock{mutex};

    gs.push_back(move(g));
}


Goroutine
Goroutine_list::release(Goroutine::Handle h)
{
    Goroutine   g;
    Lock        lock{mutex};
    auto        p = find_if(gs.begin(), gs.end(), handle_equal(h));

    if (p != gs.end()) {
        g = move(*p);
        gs.erase(p);
    }

    return g;
}


}   // Implementation Details


/*
    Goroutine Scheduler
*/
Scheduler::Scheduler()
    : workqueues{thread::hardware_concurrency()}
{
    const auto nqueues = workqueues.size();

    workers.reserve(nqueues);
    for (unsigned q = 0; q != nqueues; ++q)
        workers.emplace_back([&,q]{ run_work(q); });
}


Scheduler::~Scheduler()
{
    // TODO: This destructor should be unnecessary because...
    workqueues.interrupt();             // TODO: queues should be shutdown implicitly (in their destructor)
    for (auto& w : workers) w.join();   // TODO: workers should be joined implicity (in their destructor)
}


void
Scheduler::resume(Goroutine::Handle h)
{
    workqueues.push(suspended.release(h));
}


void
Scheduler::run_work(unsigned threadq)
{
    Goroutine   g;
    bool        is_work;

    while (true) {
        tie(g, is_work) = workqueues.pop(threadq);
        if (!is_work) break;

        try {
            g.run();
        } catch (...) {
            workqueues.interrupt();
        }
    }
}


void
Scheduler::submit(Goroutine&& g)
{
    workqueues.push(move(g));
}


void
Scheduler::suspend(Goroutine::Handle h)
{
    suspended.insert(Goroutine(h));
}


}   // Concurrency
}   // Isptech
