//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/config.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/config.hpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_CONCURRENCY_CONFIG_HPP
#define ISPTECH_CONCURRENCY_CONFIG_HPP

#include "isptech/config.hpp"


/*
    Detect API usage.
*/
# if !defined ISPTECH_CONCURRENCY_EXPORTS
#   define ISPTECH_CONCURRENCY_DECL
# else
#   if defined ISPTECH_CONCURRENCY_SOURCE
#       define ISPTECH_CONCURRENCY_DECL ISPTECH_EXPORT_DECL
#   else
#       define ISPTECH_CONCURRENCY_DECL ISPTECH_IMPORT_DECL
#   endif
#   define ISPTECH_CONCURRENCY_CALL ISPTECH_CALL
# endif


#endif  // ISPTECH_CONCURRENCY_CONFIG_HPP


//  $CUSTOM_FOOTER$
