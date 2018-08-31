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
using std::find_if;
using std::move;
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


optional<Goroutine>
Workqueue::pop()
{
    optional<Goroutine> gp;
    Lock                lock{mutex};

    while (q.is_empty() && !is_interrupt)
        ready.wait(lock);

    if (!q.is_empty())
        gp = q.pop();

    return gp;
}


void
Workqueue::push(Goroutine&& g)
{
    push(mutex, move(g), &q);
    ready.notify_one();
}


inline void
Workqueue::push(Mutex& sync, Goroutine&& g, Goroutine_queue* qp)
{
    Lock lock{sync};

    qp->push(move(g));
}


optional<Goroutine>
Workqueue::try_pop()
{
    optional<Goroutine> gp;
    Lock                lock{mutex, try_to_lock};

    if (lock && !q.is_empty())
        gp = q.pop();

    return gp;
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


optional<Goroutine>
Workqueue_array::pop(Size preferred)
{
    const auto          nqueues = queues.size();
    optional<Goroutine> gp;

    // Beginning with the preferred queue, try to dequeue work without waiting.
    for (Size i = 0; !gp && i < nqueues; ++i) {
        auto pos = (preferred + i) % nqueues;
        gp = queues[pos].try_pop();
    }

    // If we failed, wait on the preferred queue.
    if (!gp)
        gp = queues[preferred].pop();

    return gp;
}


void
Workqueue_array::push(Goroutine&& g)
{
    const auto  nqueues     = queues.size();
    const auto  preferred   = nextqueue++ % nqueues;
    bool        is_enqueued = false;

    // Beginning with the preferred queue, try to enqueue work without waiting.
    for (Size i = 0; !is_enqueued && i < nqueues; ++i) {
        auto pos = (preferred + i) % nqueues;
        if (queues[pos].try_push(move(g)))
            is_enqueued = true;
    }

    // If we failed, wait on the preferred queue.
    if (!is_enqueued)
        queues[preferred].push(move(g));
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
    Channel Operation
*/
Channel_operation::Channel_operation()
    : kind(none)
    , chanp{nullptr}
    , rvalp{nullptr}
    , lvalp{nullptr}
    , pos{0}
{
}


Channel_operation::Channel_operation(Interface* channelp, const void* rvaluep)
    : kind(send)
    , chanp{channelp}
    , rvalp{rvaluep}
    , lvalp{nullptr}
    , pos{0}
{
}


Channel_operation::Channel_operation(Interface* channelp, Type optype, void* lvaluep)
    : kind(optype)
    , chanp{channelp}
    , rvalp{nullptr}
    , lvalp{lvaluep}
    , pos{0}
{
}


void
Channel_operation::enqueue(Goroutine::Handle g)
{
    if (chanp && g) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->enqueue_send(g, rvalp);
            else if (lvalp)
                chanp->enqueue_send(g, lvalp);
            break;

        case receive:
            if (lvalp)
                chanp->enqueue_receive(g, lvalp);
            break;
        }
    }
}


void
Channel_operation::execute()
{
    if (chanp) {
        switch(kind) {
        case send:
            if (rvalp)
                chanp->ready_send(rvalp);
            else if (lvalp)
                chanp->ready_send(lvalp);
            break;

        case receive:
            if (lvalp)
                chanp->ready_receive(lvalp);
            break;
        }
    }
}


bool
Channel_operation::is_ready() const
{
    if (!chanp) return false;

    switch(kind) {
    case send:      return chanp->is_sendable(); break;
    case receive:   return chanp->is_receivable(); break;
    default:        return false;
    }
}


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
Scheduler::run_work(unsigned threadpos)
{
    while (optional<Goroutine> gp = workqueues.pop(threadpos)) {
        try {
            gp->run();
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
