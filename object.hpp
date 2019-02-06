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

#include "boost/operators.hpp"
#include <cstddef>


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
using Object_type           = std::ptrdiff_t;
using Object_instance       = std::ptrdiff_t;
using Member_function_id    = int;


/*
    Object Identity
*/
class Object_id : boost::totally_ordered<Object_id> {
public:
    // Construct
    constexpr Object_id();
    constexpr Object_id(Object_type, Object_instance);

    // Modifiers
    constexpr void type(Object_type);
    constexpr void instance(Object_instance);

    // Observers
    constexpr Object_type       type() const;
    constexpr Object_instance   instance() const;

    // Comparisons
    friend constexpr bool operator==(Object_id, Object_id);
    friend constexpr bool operator< (Object_id, Object_id);

private:
    // Data
    Object_type        what;
    Object_instance    which;
};


/*
    Member Function Type
*/
enum class Member_function_type : int {
    normal,
    idempotent
};


/*
    Member Function
*/
class Member_function : boost::totally_ordered<Member_function> {
public:
    // Construct
    constexpr Member_function();
    constexpr Member_function(Member_function_id, Member_function_type);

    // Modifiers
    constexpr void id(Member_function_id);
    constexpr void type(Member_function_type);

    // Observers
    constexpr Member_function_id    id() const;
    constexpr Member_function_type  type() const;

    // Comparisons
    friend constexpr bool operator==(Member_function, Member_function);
    friend constexpr bool operator< (Member_function, Member_function);

private:
    // Data
    Member_function_id     which;
    Member_function_type   kind;
};


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_OBJECT_HPP

//  $CUSTOM_FOOTER$
