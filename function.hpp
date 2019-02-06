//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/orb/function.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/orb/function.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_ORB_FUNCTION_HPP
#define ISPTECH_ORB_FUNCTION_HPP

#include "boost/operators.hpp"


/*
    Information and Sensor Processing Technology Object Request Broker
*/
namespace Isptech   {
namespace Orb       {


/*
    Names/Types
*/
using Function_id = int;


/*
    Function Type
*/
enum class Function_type : int {
    normal,
    idempotent
};


/*
    Function
*/
class Function : boost::totally_ordered<Function> {
public:
    // Construct
    constexpr Function();
    constexpr Function(Function_id, Function_type);

    // Modifiers
    constexpr void id(Function_id);
    constexpr void type(Function_type);

    // Observers
    constexpr Function_id   id() const;
    constexpr Function_type type() const;

    // Comparisons
    friend constexpr bool operator==(Function, Function);
    friend constexpr bool operator< (Function, Function);

private:
    // Data
    Function_id     which;
    Function_type   kind;
};


using Member_function = Function;


}   // Orb
}   // Isptech


#endif  // ISPTECH_ORB_FUNCTION_HPP

//  $CUSTOM_FOOTER$
