//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/object_impl.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/object_impl.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_OBJECT_IMPL_HPP
#define ISPTECH_ORB_OBJECT_IMPL_HPP

#include "isptech/orb/buffer.hpp"
#include "isptech/orb/function.hpp"
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
    Object Implementation
*/
class Object_impl : boost::totally_ordered<Object_impl> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;

    // Construct/Copy/Move
    Object_impl() = default;
    Object_impl(Interface_ptr);
    Object_impl& operator=(Object_impl);
    Object_impl(Object_impl&&);
    friend void swap(Object_impl&, Object_impl&);

    // Function Invocation
    Future<bool> invoke(Object_id, Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool> invoke(Object_id, Function, Const_buffers in, Mutable_buffers* outp) const;

    // Comparisons
    friend bool operator==(const Object_impl&, const Object_impl&);
    friend bool operator< (const Object_impl&, const Object_impl&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Object Implementation Interface
*/
class Object_impl::Interface {
public:
    // Copy/Destroy
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Function Invocation
    virtual Future<bool> invoke(Object_id, Function, Const_buffers in, Io_buffer* outp) = 0;
    virtual Future<bool> invoke(Object_id, Function, Const_buffers in, Mutable_buffers* outp) = 0;
};


}   // Orb
}   // Isptech


// External Definitions
#include "isptech/orb/object_impl.inl"

#endif  // ISPTECH_ORB_OBJECT_HPP
