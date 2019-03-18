//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_OBJECT_HPP
#define ISPTECH_ORB_OBJECT_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function_id.hpp"
#include "isptech/orb/object_id.hpp"
#include "isptech/orb/future.hpp"
#include "boost/operators.hpp"
#include <memory>
#include <utility>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
class Object_map;


/*
    Object Dispatcher
*/
class Object_dispatcher : boost::totally_ordered<Object_dispatcher> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Object_dispatcher(Interface_ptr = Interface_ptr());
    Object_dispatcher& operator=(Object_dispatcher);
    Object_dispatcher(Object_dispatcher&&);
    friend void swap(Object_dispatcher&, Object_dispatcher&);

    // Function Invocation
    Future<bool> invoke(Object_id, Function_id, Io_buffer* iop, const Object_map&) const;

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Object_dispatcher&, const Object_dispatcher&);
    friend bool operator< (const Object_dispatcher&, const Object_dispatcher&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Object Dispatcher Interface
*/
class Object_dispatcher::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Function Invocation
    virtual Future<bool> invoke(Object_id, Function_id, Io_buffer* iop, const Object_map&) = 0;
};


/*
    Object
*/
class Object : boost::totally_ordered<Object> {
public:
    // Construct/Copy/Move
    Object() = default;
    explicit Object(Object_dispatcher, Object_id = Object_id());
    Object& operator=(Object);
    Object(Object&&);
    friend void swap(Object&, Object&);

    // Identity
    void        identity(Object_id);
    Object_id   identity() const;

    // Implementation
    void                        dispatcher(Object_dispatcher);
    const Object_dispatcher&    dispatcher() const;

    // Function Invocation
    Future<bool> invoke(Function_id, Io_buffer* iop, const Object_map&) const;

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Object&, const Object&);
    friend bool operator< (const Object&, const Object&);

private:
    // Data
    Object_id           obj;
    Object_dispatcher   impl;
};


/*
    Object Map
*/
class Object_map : boost::totally_ordered<Object_map> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Object_map(Interface_ptr = Interface_ptr());
    Object_map(const Object_map&&);
    Object_map& operator=(Object_map);
    friend void swap(Object_map&, Object_map&);

    // Map Functions
    void    insert(Object);
    Object  find(Object_id) const;
    Object  remove(Object_id);

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Object_map&, const Object_map&);
    friend bool operator< (const Object_map&, const Object_map&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Object Map Interface
*/
class Object_map::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Map Functions
    virtual void    insert(Object) = 0;
    virtual Object  find(Object_id) const = 0;
    virtual Object  remove(Object_id) = 0;
};


}   // Orb
}   // Isptech


// External Definitions
#include "isptech/orb/object.inl"

#endif  // ISPTECH_ORB_OBJECT_HPP

//  $CUSTOM_FOOTER$
