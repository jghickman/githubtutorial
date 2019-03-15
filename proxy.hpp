//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/proxy.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/proxy.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_PROXY_HPP
#define ISPTECH_ORB_PROXY_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function.hpp"
#include "isptech/orb/object_id.hpp"
#include "isptech/orb/future.hpp"
#include "boost/operators.hpp"
#include <memory>

#include <vector> // for sketching


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


class Dynamic_buffer {
public:
    void* resize(int) const;
};
    

template<class T> Dynamic_buffer dynamic_buffer(T&);

void
copy(void* in, int n, Dynamic_buffer_view out)
{
    if (!out.resize(n)) throw Buffer_overflow();
}


/*
    Two-Way Proxy
*/
class Twoway_proxy : boost::totally_ordered<Twoway_proxy> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Twoway_proxy() = default;
    Twoway_proxy(Interface_ptr);
    Twoway_proxy& operator=(Twoway_proxy);
    Twoway_proxy(Twoway_proxy&&);
    friend void swap(Twoway_proxy&, Twoway_proxy&);

    // Function Invocation
    Future<bool> invoke(Object_id, Function) const;                                         // input=n, output=n
    Future<bool> invoke(Object_id, Function, const Dynamic_buffer& out) const;              // input=n, output=y
    Future<bool> invoke(Object_id, Function, Const_buffers in) const;                       // input=y, output=n
    Future<bool> invoke(Object_id, Function, Const_buffers in, Dynamic_buffer out) const;   // input=y, output=y


    Future<bool> invoke(Object_id, Function, Mutable_buffers out) const; // input=n, output=y

    // Comparisons
    friend bool operator==(const Twoway_proxy&, const Twoway_proxy&);
    friend bool operator< (const Twoway_proxy&, const Twoway_proxy&);

private:
    // Data
    Interface_ptr ifacep;
};


class File {
public:
    int read(char* bufferp);        // read max of fixed-size buffer
    int read(Mutable_buffers);      // read max of fixed-size buffers
    int read(std::vector<char>*);   // read until end-of-input

    void    set_id(int);
    int     get_id() const;

    Future<void> read(std::vector<int>* vp) {
        proxy.invoke(object, read_int_vector, dynamic_buffer(*vp));

        Channel<void> 
    }

private:
    Twoway_proxy    proxy;
    Object_id       object;
    Function        read_int_vector;
};


/*
    Two-Way Proxy Interface
*/
class Twoway_proxy::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Function Invocation
    virtual Future<bool> invoke(Object_id, Function, Const_buffer in, Io_buffer* outp) = 0;
    virtual Future<bool> invoke(Object_id, Function, Const_buffer in, Mutable_buffers* outp) = 0;
};


/*
    Two-Way Object Proxy
*/
class Twoway_object_proxy : boost::totally_ordered<Twoway_object_proxy> {
public:
    // Construct/Copy/Move
    Twoway_object_proxy() = default;
    explicit Twoway_object_proxy(Twoway_proxy, Object_id = Object_id());
    Twoway_object_proxy& operator=(const Twoway_object_proxy&) = default;
    Twoway_object_proxy(Twoway_object_proxy&&);
    Twoway_object_proxy& operator=(Twoway_object_proxy&&);
    friend void swap(Twoway_object_proxy&, Twoway_object_proxy&);

    // Object Identity
    void        identity(Object_id) const;
    Object_id   identity() const;

    // Implementation
    void                base(Twoway_proxy)
    const Twoway_proxy& base() const;

    // Function Invocation
    Future<bool> invoke(Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool> invoke(Function, Const_buffers in, Mutable_buffers* outp) const;

    // Blocking Function Invocation
    friend bool blocking_invoke(const Twoway_object_proxy&, Function, Const_buffers in, Io_buffer* outp);
    friend bool blocking_invoke(const Twoway_object_proxy&, Function, Const_buffers in, Mutable_buffers* outp);

    // Comparisons
    friend bool operator==(const Twoway_object_proxy&, const Twoway_object_proxy&);
    friend bool operator< (const Twoway_object_proxy&, const Twoway_object_proxy&);

private:
    // Data
    Object_id       id;
    Twoway_proxy    proxy;
};


}   // Orb
}   // Isptech

#endif  // ISPTECH_ORB_PROXY_HPP

//  $CUSTOM_FOOTER$
