/* $OpenLDAP$ */
/* root_dse.c - Provides the ROOT DSA-Specific Entry
 *
 * Copyright 1999-2000 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted only
 * as authorized by the OpenLDAP Public License.  A copy of this
 * license is available at http://www.OpenLDAP.org/license.html or
 * in file LICENSE in the top-level directory of the distribution.
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "slap.h"

int
root_dse_info( Entry **entry, char **text )
{
	char buf[BUFSIZ];
	Entry		*e;
	struct berval	val;
	struct berval	*vals[2];
	int		i, j;

#ifdef SLAPD_SCHEMA_NOT_COMPAT
	AttributeDescription *ad_objectClass = slap_schema.si_ad_objectClass;
	AttributeDescription *ad_namingContexts = slap_schema.si_ad_namingContexts;
	AttributeDescription *ad_supportedControl = slap_schema.si_ad_supportedControl;
	AttributeDescription *ad_supportedExtension = slap_schema.si_ad_supportedExtension;
	AttributeDescription *ad_supportedLDAPVersion = slap_schema.si_ad_supportedLDAPVersion;
	AttributeDescription *ad_supportedSASLMechanisms = slap_schema.si_ad_supportedSASLMechanisms;
#	ifdef SLAPD_ACI_ENABLED
	AttributeDescription *ad_supportedACIMechanisms = slap_schema.si_ad_supportedACIMechanisms;
#	endif
	AttributeDescription *ad_ref = slap_schema.si_ad_ref;
#else
	char *ad_objectClass = "objectClass";
	char *ad_namingContexts = "namingContexts";
	char *ad_supportedControl = "supportedControl";
	char *ad_supportedExtension = "supportedExtension";
	char *ad_supportedLDAPVersion = "supportedLDAPVersion";
	char *ad_supportedSASLMechanisms = "supportedSASLMechanisms";
#	ifdef SLAPD_ACI_ENABLED
	char *ad_supportedACIMechanisms = "supportedACIMechanisms";
#	endif
	char *ad_ref = "ref";
#endif

	vals[0] = &val;
	vals[1] = NULL;

	e = (Entry *) ch_calloc( 1, sizeof(Entry) );

	e->e_attrs = NULL;
	e->e_dn = ch_strdup( LDAP_ROOT_DSE );
	e->e_ndn = ch_strdup( LDAP_ROOT_DSE );
	(void) dn_normalize( e->e_ndn );
	e->e_private = NULL;

	val.bv_val = "top";
	val.bv_len = sizeof("top")-1;
	attr_merge( e, ad_objectClass, vals );

	val.bv_val = "OpenLDAProotDSE";
	val.bv_len = sizeof("OpenLDAProotDSE")-1;
	attr_merge( e, ad_objectClass, vals );

	for ( i = 0; i < nbackends; i++ ) {
		for ( j = 0; backends[i].be_suffix[j] != NULL; j++ ) {
			val.bv_val = backends[i].be_suffix[j];
			val.bv_len = strlen( val.bv_val );
			attr_merge( e, ad_namingContexts, vals );
		}
	}

	/* altServer unsupported */

	/* supportedControl */
	for ( i=0; supportedControls[i] != NULL; i++ ) {
		val.bv_val = supportedControls[i];
		val.bv_len = strlen( val.bv_val );
		attr_merge( e, ad_supportedControl, vals );
	}

	/* supportedExtension */
	for ( i=0; (val.bv_val = get_supported_extop(i)) != NULL; i++ ) {
		val.bv_len = strlen( val.bv_val );
		attr_merge( e, ad_supportedExtension, vals );
	}

	/* supportedLDAPVersion */
	for ( i=LDAP_VERSION_MIN; i<=LDAP_VERSION_MAX; i++ ) {
		sprintf(buf,"%d",i);
		val.bv_val = buf;
		val.bv_len = strlen( val.bv_val );
		attr_merge( e, ad_supportedLDAPVersion, vals );
	}

	/* supportedSASLMechanism */
	if( supportedSASLMechanisms != NULL ) {
		for ( i=0; supportedSASLMechanisms[i] != NULL; i++ ) {
			val.bv_val = supportedSASLMechanisms[i];
			val.bv_len = strlen( val.bv_val );
			attr_merge( e, ad_supportedSASLMechanisms, vals );
		}
	}

#ifdef SLAPD_ACI_ENABLED
	/* supportedACIMechanisms */
	for ( i=0; (val.bv_val = get_supported_acimech(i)) != NULL; i++ ) {
		val.bv_len = strlen( val.bv_val );
		attr_merge( e, ad_supportedACIMechanisms, vals );
	}
#endif

	if ( default_referral != NULL ) {
		attr_merge( e, ad_ref, default_referral );
	}

	*entry = e;
	return LDAP_SUCCESS;
}

