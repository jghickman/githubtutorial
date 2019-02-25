//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/config.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:53:01 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/coroutine/config.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_COROUTINE_CONFIG_HPP
#define ISPTECH_COROUTINE_CONFIG_HPP

#include "isptech/config.hpp"


/*
    Detect API usage.
*/
# if !defined ISPTECH_COROUTINE_EXPORTS
#   define ISPTECH_COROUTINE_DECL
# else
#   if defined ISPTECH_COROUTINE_SOURCE
#       define ISPTECH_COROUTINE_DECL ISPTECH_EXPORT_DECL
#   else
#       define ISPTECH_COROUTINE_DECL ISPTECH_IMPORT_DECL
#   endif
#   define ISPTECH_COROUTINE_CALL ISPTECH_CALL
# endif


#endif  // ISPTECH_COROUTINE_CONFIG_HPP

//  $CUSTOM_FOOTER$
