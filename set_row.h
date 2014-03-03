/*
	ODBO provider for XMLA data stores
    Copyright (C) 2014  Yalos Software Labs
	http://www.arquery.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

	@description
					a row in the set rowset
*/

#pragma once

#include "soapH.h"

class set_row
{
public:
	wchar_t         m_catalog[256];
	wchar_t         m_schema[256];
	wchar_t         m_cube[256];
	wchar_t         m_set_name[256];
	long            m_scope;
	wchar_t         m_description[256];

	set_row( row& a_row )
	{

		m_schema[0]=0;
		m_set_name[0]=0;//TODO

		wcscpy_s( m_catalog, 256, FROM_STRING( a_row.CATALOG_USCORENAME, CP_UTF8 ) );
		wcscpy_s( m_cube, 256, FROM_STRING( a_row.CUBE_USCORENAME, CP_UTF8 ) );
		wcscpy_s( m_description, 256, FROM_STRING( a_row.DESCRIPTION, CP_UTF8 ) );

		m_scope = 0;//TODO
	}

	EMPTY_CONSTRUCTOR(set_row);

	static char* schema_name() { return "UNSUPORTED"; }

	BEGIN_PROVIDER_COLUMN_MAP( set_row )
	PROVIDER_COLUMN_ENTRY_WSTR( "CATALOG_NAME", 1, m_catalog )
	PROVIDER_COLUMN_ENTRY_WSTR( "SCHEMA_NAME", 2, m_schema )
	PROVIDER_COLUMN_ENTRY_WSTR( "CUBE_NAME", 3, m_cube )
	PROVIDER_COLUMN_ENTRY_WSTR( "SET_NAME", 4, m_set_name )
	PROVIDER_COLUMN_ENTRY_TYPE( "SCOPE", 5, VT_I4, m_scope )
	PROVIDER_COLUMN_ENTRY_WSTR( "DESCRIPTION", 6, m_description )
	END_PROVIDER_COLUMN_MAP()
};