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

#include "isptech/orb/object_impl.hpp"
#include "boost/operators.hpp"


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Object

    TODO:  Would there be merit in including an indication of const-ness in
    an object's invocation interface to reflect the const-ness of the
    underlying member function?
*/
class Object : boost::totally_ordered<Object> {
public:
    // Construct/Copy/Move
    Object() = default;
    explicit Object(Object_impl, Object_id = Object_id());
    Object& operator=(Object);
    Object(Object&&);
    friend void swap(Object&, Object&);

    // Identity
    void        identity(Object_id);
    Object_id   identity() const;

    // Implementation
    void                base(Object_impl);
    const Object_impl&  base() const;

    // Function Invocation
    Future<bool> invoke(Function, Const_buffers in, Io_buffer* outp) const;
    Future<bool> invoke(Function, Const_buffers in, Mutable_buffers* outp) const;

    // Comparisons
    friend bool operator==(const Object&, const Object&);
    friend bool operator< (const Object&, const Object&);

private:
    // Data
    Object_id   id;
    Object_impl impl;
};


}   // Orb
}   // Isptech


// External Definitions
#include "isptech/orb/object.inl"

#endif  // ISPTECH_ORB_OBJECT_HPP

//  $CUSTOM_FOOTER$
