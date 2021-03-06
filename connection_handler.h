/*
	ODBO provider for XMLA data stores
    Copyright (C) 2014-2015  ARquery LTD
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
					wraps a connection to the XMXLA server.
					calls Discover/Execute.
					intended for one single call to the server.
*/

#pragma once

#define ALLOW_TRANSLATIONS

#include <vector>
#include <unordered_map>
#include <cstring>
#include "soapXMLAConnectionProxy.h"


#include "query_translator.h"


#include <cctype>


#include "config_data.h"
#include "dimension_properties.h"

class connection_handler
{
public:
	static const unsigned int WORD_WIDTH = 256;
	class tabular_data_access
	{
	private:
		connection_handler& m_handler;
		int m_col_count;
		ATLCOLUMNINFO* m_columns;

		void make_col_info()
		{
			m_columns = new ATLCOLUMNINFO[ m_col_count ];
			size_t offset = 0;
			for( int i = 0; i < m_col_count; ++i )
			{
				m_columns[i].pwszName = _wcsdup( CA2W(tabular_header( i ), CP_UTF8) );
				m_columns[i].pTypeInfo = (ITypeInfo*)NULL; 
				m_columns[i].iOrdinal = (ULONG)(i + 1); 
				m_columns[i].dwFlags = 0; 
				m_columns[i].wType = (DBTYPE) tabular_header_type(i); 
				switch ( m_columns[i].wType )
				{
				case DBTYPE_WSTR:
					m_columns[i].ulColumnSize = (ULONG)WORD_WIDTH; 
					m_columns[i].bPrecision = (BYTE)0xFF;
					m_columns[i].bScale = (BYTE)0xFF;
					m_columns[i].cbOffset = offset;
					offset += WORD_WIDTH * sizeof(wchar_t);
					break;
				case DBTYPE_R8:
					m_columns[i].ulColumnSize = (ULONG)sizeof(double); 
					m_columns[i].bPrecision = (BYTE)0xFF; 
					m_columns[i].bScale = (BYTE)0xFF;
					m_columns[i].cbOffset = offset;
					offset += sizeof(double);
					break;
				case DBTYPE_I4:
					m_columns[i].ulColumnSize = (ULONG)sizeof(int); 
					m_columns[i].bPrecision = (BYTE)0xFF;
					m_columns[i].bScale = (BYTE)0xFF;
					m_columns[i].cbOffset = offset;
					offset += sizeof(int);
					break;
				}
			}
		}

		void clean_column_info()
		{
			if ( nullptr == m_columns ){return;}
	
			for ( int i = 0; i < m_col_count; ++i )
			{
				delete[] m_columns[i].pwszName;
			}
			m_col_count = 0;
			delete[] m_columns;
			m_columns = nullptr;
		}

		const char* tabular_header( const int idx )
		{
			if ( 0 == m_handler.m_e_response.cxmla__return__.root.__size ) { return ""; }//preffer empty to null
			if ( idx >= m_handler.m_e_response.cxmla__return__.root.row[0].__size ){ return ""; }//preffer empty to null
			return m_handler.m_e_response.cxmla__return__.root.row[0].__array[idx].elementName;
		}
		const DBTYPEENUM tabular_header_type( const int idx )
		{
			//the first row contains headers and counts. does not have typeinfo on the tags.
			if ( 2 > m_handler.m_e_response.cxmla__return__.root.__size ) { return DBTYPE_WSTR; }//unknown is string
			if ( idx >= m_handler.m_e_response.cxmla__return__.root.row[1].__size ){ return DBTYPE_WSTR; }//unknown is string
			if ( nullptr == (m_handler.m_e_response.cxmla__return__.root.row[1].__array[idx].__xsi__type ) ) { return DBTYPE_WSTR; }//unknown is string
			const std::string type(m_handler.m_e_response.cxmla__return__.root.row[1].__array[idx].__xsi__type);
			if ( type == "xsd:double" ) { return DBTYPE_R8; }
			if ( type == "xsd:int" ) { return DBTYPE_I4; }
			return DBTYPE_WSTR;
		}
	public:
		tabular_data_access( connection_handler& handler ) 
			: m_handler( handler )
			, m_columns( nullptr )
		{
			if ( 0 == m_handler.m_e_response.cxmla__return__.root.__size ) 
			{
				m_col_count = 0;
				return;
			}

			if ( nullptr == m_handler.m_e_response.cxmla__return__.root.row )
			{
				m_col_count = 0;
				return;
			}

			m_col_count = m_handler.m_e_response.cxmla__return__.root.row[0].__size;
			make_col_info();
		}

		~tabular_data_access()
		{
			clean_column_info();
		}

		ATLCOLUMNINFO* GetColumnInfo( DBORDINAL* pcCols )
		{
			* pcCols = m_col_count;
			return m_columns;
		}

		void load_at( int idx, wchar_t* data )
		{
			if ( 0 > idx ) { return; }
			if ( idx >= m_handler.m_e_response.cxmla__return__.root.__size ){ return; }

			for( int i = 0; i < m_col_count; ++i )
			{
				switch ( m_columns[i].wType )
				{
				case DBTYPE_WSTR:
					wcscpy_s( ( wchar_t*) ((char*)data + m_columns[i].cbOffset), WORD_WIDTH, CA2W(m_handler.m_e_response.cxmla__return__.root.row[idx].__array[i].value, CP_UTF8) );
					break;
				case DBTYPE_R8:
					{
						double val = atof( m_handler.m_e_response.cxmla__return__.root.row[idx].__array[i].value );
						CopyMemory( (char*)data + m_columns[i].cbOffset, &val, sizeof( val ) );
					}
					break;
				case DBTYPE_I4:
					{
						int val = atoi( m_handler.m_e_response.cxmla__return__.root.row[idx].__array[i].value );
						CopyMemory( (char*)data + m_columns[i].cbOffset, &val, sizeof( val ) );
					}
					break;
				}
			}

			//return m_handler.m_e_response.cxmla__return__.root.row[0].__array[idx].elementName;
		}

		const int col_count() const { return m_col_count; }
		const int row_count() const { return m_handler.m_e_response.cxmla__return__.root.__size; }
		const int data_size() const { return m_col_count * WORD_WIDTH * sizeof(wchar_t); }
	};
public:
	class out_of_bound : public std::runtime_error 
	{
	public:
		out_of_bound() : std::runtime_error("index out of bounds"){}
	};
private:
	int m_session_id;
	cxmla__DiscoverResponse m_d_response;
	cxmla__ExecuteResponse m_e_response;
	session* m_session;
	XMLAConnectionProxy m_proxy;
	std::string m_location;
	std::string m_user;
	std::string m_pass;
	std::string m_catalog;
	std::vector<int> m_cell_data;

	tabular_data_access* m_tab_data_access;

	ATL::ATLCOLUMNINFO* m_execute_colls;
	size_t m_cell_ordinal_pos;
	size_t m_execute_col_count;
	std::vector< std::pair< std::string, int > > m_indirection;//0 will be value, all user props will substract 1
private:

	typedef std::unordered_map< soap*, session* > indirection_table_type;
	static indirection_table_type& soap_2_session()
	{
		static indirection_table_type result;
		return result;
	}
	static int (*fparsehdr)(struct soap*, const char*, const char*);//this is defined in command.cpp

	static int http_post_parse_header(struct soap *soap, const char* key, const char* val)
	{
		if ( !soap_tag_cmp(key, "Server") )
		{
			::session* match = soap_2_session()[soap];
			if ( nullptr != match ) 
			{
				session::session_table()[ match ].register_server( val );
			}
		}
		return fparsehdr( soap, key, val );
	}

	HRESULT get_connection_data()
	{
		HRESULT					hr;
		IGetDataSource*			pDataSource = NULL;
		IDBProperties*			pProperties = NULL;
		IMalloc*				pIMalloc = NULL;

		ULONG propCount;
		DBPROPSET* props;

		if FAILED( hr = CoGetMalloc( 1, &pIMalloc ) ) {
			return hr;
		}

		if FAILED( hr = m_session->QueryInterface(__uuidof(IGetDataSource),(void**)&pDataSource) ) {
			pIMalloc->Release();
			return hr;
		}

		if FAILED( hr = pDataSource->GetDataSource( __uuidof(IDBProperties), ( IUnknown** ) &pProperties ) )
		{
			pIMalloc->Release();
			pDataSource->Release();
			return hr;
		}

		//Session catalog has lower precendence than db catalog
		ISessionProperties* pISessionProperties = NULL;
		if SUCCEEDED( m_session->QueryInterface(__uuidof(ISessionProperties),(void**)&pISessionProperties) ) {
			pISessionProperties->GetProperties( 0, NULL, &propCount, &props );

			for ( ULONG i = 0; i < propCount; i++ )
			{
				for ( ULONG j =0; j < props[i].cProperties; j++ )
				{
					if ( IsEqualGUID( props[i].guidPropertySet, DBPROPSET_SESSION ) ) {
						if ( DBPROP_CURRENTCATALOG == props[i].rgProperties[j].dwPropertyID ) {
							std::string buf = CT2A(props[i].rgProperties[j].vValue.bstrVal, CP_UTF8);
							if ( !buf.empty() )  {
								std::swap( m_catalog, buf );
							}
						}
					}
					VariantClear( &(props[i].rgProperties[j].vValue) );
				}
				pIMalloc->Free( props[i].rgProperties );
			}
			pIMalloc->Free( props );

			pISessionProperties->Release();
		}


		pProperties->GetProperties( 0, NULL, &propCount, &props );

		for ( ULONG i = 0; i < propCount; i++ )
		{
			for ( ULONG j =0; j < props[i].cProperties; j++ )
			{
				if ( IsEqualGUID( props[i].guidPropertySet,DBPROPSET_DBINIT ) )
				{
					switch ( props[i].rgProperties[j].dwPropertyID )
					{
					case DBPROP_INIT_LOCATION:
						m_location = CT2A(props[i].rgProperties[j].vValue.bstrVal, CP_UTF8);
						break;
					case DBPROP_AUTH_USERID:
						m_user = CT2A(props[i].rgProperties[j].vValue.bstrVal, CP_UTF8);
						break;
					case DBPROP_AUTH_PASSWORD:
						m_pass = CT2A(props[i].rgProperties[j].vValue.bstrVal, CP_UTF8);
						break;
					case DBPROP_INIT_CATALOG:
						if ( props[i].rgProperties[j].vValue.bstrVal )  m_catalog = CT2A(props[i].rgProperties[j].vValue.bstrVal, CP_UTF8);
						break;
					}
				}
				VariantClear( &(props[i].rgProperties[j].vValue) );
			}
			pIMalloc->Free( props[i].rgProperties );
		}
		pIMalloc->Free( props );

		pIMalloc->Release();
		pProperties->Release();
		pDataSource->Release();
		return S_OK;
	}

	void load_restrictions( ULONG cRestrictions, const VARIANT* rgRestrictions, xmlns__Restrictions& where, bool add_cat = true )
	{
		//TODO: validate memory consumption due to the strdup here

		where.RestrictionList.PropertyName = NULL;
		where.RestrictionList.CATALOG_USCORENAME = NULL;
		where.RestrictionList.CUBE_USCORENAME = NULL;
		where.RestrictionList.HIERARCHY_USCOREUNIQUE_USCORENAME = NULL;
		where.RestrictionList.MEMBER_USCOREUNIQUE_USCORENAME = NULL;
		where.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME = NULL;
		where.RestrictionList.TREE_USCOREOP = NULL;
		where.RestrictionList.PROPERTY_USCORETYPE = NULL;

		if ( !m_catalog.empty() && add_cat ) {
			where.RestrictionList.CATALOG_USCORENAME = _strdup(m_catalog.c_str());//Allocates for consistence (all the other do strdup)
		}

		//only handle what we know
		for( ULONG i = 0; i < cRestrictions; i++ ) 
		{
			switch (i) 
			{
			case 0://CATALOG_NAME
				if ( VT_BSTR == rgRestrictions[i].vt  ) {
					where.RestrictionList.CATALOG_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				}
				break;
			case 2://CUBE_NAME
				if ( VT_BSTR == rgRestrictions[i].vt  ) {
					where.RestrictionList.CUBE_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				}
				break;
			case 3://SET_NAME
				if ( VT_BSTR == rgRestrictions[i].vt && add_cat ) {
					where.RestrictionList.SET_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				}
				break;
			case 4://HIERARCHY_UNIQUE_NAME
				if ( VT_BSTR == rgRestrictions[i].vt  ) {
					where.RestrictionList.HIERARCHY_USCOREUNIQUE_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				}
				break;
			case 5://LEVEL_UNIQUE_NAME
				if ( VT_BSTR == rgRestrictions[i].vt  ) {
					where.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				}
				break;
			case 8:
				if ( VT_BSTR == rgRestrictions[i].vt  ) {//MEMBER_UNIQUE_NAME
					where.RestrictionList.MEMBER_USCOREUNIQUE_USCORENAME = _strdup(CT2A(rgRestrictions[i].bstrVal, CP_UTF8));
				} else if ( VT_I2 == rgRestrictions[i].vt ) {//PROPERTY_TYPE
					char buf[20];
					_itoa_s( rgRestrictions[i].iVal, buf, 20, 10 );
					where.RestrictionList.PROPERTY_USCORETYPE = _strdup( buf );
				}
				break;
			case 11://TREE_OP
				if ( VT_UI4 == rgRestrictions[i].vt  ) {
					char buf[20];
					_itoa_s( rgRestrictions[i].uiVal, buf, 20, 10 );
					where.RestrictionList.TREE_USCOREOP = _strdup( buf );
				} //else if ( VT_EMPTY == rgRestrictions[i].vt  ) {
				//	where.RestrictionList.TREE_USCOREOP = "0";
				//}
				break;
			}
		}
	}

	void unload_restrictions( xmlns__Restrictions& where )
	{
		if ( NULL != where.RestrictionList.PROPERTY_USCORETYPE ) 
		{
			free( where.RestrictionList.PROPERTY_USCORETYPE );
			where.RestrictionList.PROPERTY_USCORETYPE = NULL;
		}
		if ( NULL != where.RestrictionList.PropertyName ) 
		{
			free( where.RestrictionList.PropertyName );
			where.RestrictionList.PropertyName = NULL;
		}
		if ( NULL != where.RestrictionList.CATALOG_USCORENAME )
		{
			free( where.RestrictionList.CATALOG_USCORENAME );
			where.RestrictionList.CATALOG_USCORENAME = NULL;
		}
		if ( NULL != where.RestrictionList.CUBE_USCORENAME )
		{
			free( where.RestrictionList.CUBE_USCORENAME );
			where.RestrictionList.CUBE_USCORENAME = NULL;
		}
		if ( NULL != where.RestrictionList.HIERARCHY_USCOREUNIQUE_USCORENAME )
		{
			free( where.RestrictionList.HIERARCHY_USCOREUNIQUE_USCORENAME );
			where.RestrictionList.HIERARCHY_USCOREUNIQUE_USCORENAME = NULL;
		}
		if ( NULL != where.RestrictionList.MEMBER_USCOREUNIQUE_USCORENAME )
		{
			free( where.RestrictionList.MEMBER_USCOREUNIQUE_USCORENAME );
			where.RestrictionList.MEMBER_USCOREUNIQUE_USCORENAME = NULL;
		}
		if ( NULL != where.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME )
		{
			free( where.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME );
			where.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME = NULL;
		}
		if ( NULL != where.RestrictionList.TREE_USCOREOP )
		{
			free( where.RestrictionList.TREE_USCOREOP );
			where.RestrictionList.TREE_USCOREOP = NULL; 
		}
		if ( NULL != where.RestrictionList.SET_USCORENAME )
		{
			free( where.RestrictionList.SET_USCORENAME );
			where.RestrictionList.SET_USCORENAME = NULL;
		}
	}

private:
	void begin_session()
	{
		m_proxy.header = new SOAP_ENV__Header();
		m_proxy.header->BeginSession = new BSessionType();
		m_proxy.header->BeginSession->element = NULL;
		m_proxy.header->BeginSession->xmlns = NULL;
		m_proxy.header->EndSession = NULL;
		m_proxy.header->Session = NULL;
	}

	void session()
	{
		char sessid[20];
		_itoa_s( m_session_id, sessid, 20, 10 );
		m_proxy.header = new SOAP_ENV__Header();
		m_proxy.header->Session = new SessionType();
		m_proxy.header->Session->element = NULL;
		m_proxy.header->Session->xmlns = NULL;
		m_proxy.header->Session->SessionId = _strdup( sessid );
		m_proxy.header->EndSession = NULL;
		m_proxy.header->BeginSession = NULL;
	}

	void safe_delete_column_headers()
	{
		if ( nullptr != m_execute_colls ) 
		{ 
			for ( size_t i = 0; i < m_execute_col_count; ++i )
			{
				delete[] m_execute_colls[i].pwszName;
			}
			delete[] m_execute_colls;
			m_execute_colls = nullptr;
		}
		m_indirection.clear();
	}

	DBTYPEENUM dbtype_from_xsd( const char* xsd )
	{
		if ( nullptr == xsd ) { return DBTYPE_WSTR; }
		if ( 0 == strcmp( "xsd:string", xsd ) ) { return DBTYPE_WSTR; }
		if ( 0 == strcmp( "xsd:int", xsd ) ) { return DBTYPE_I4; }
		if ( 0 == strcmp( "xsd:unsignedInt", xsd ) ) { return DBTYPE_UI4; }
		if ( 0 == strcmp( "xsd:short", xsd ) ) { return DBTYPE_I2; }
		if ( 0 == strcmp( "xsd:unsignedShort", xsd ) ) { return DBTYPE_UI2; }
		return DBTYPE_WSTR;
	}

	void form_column_headers()
	{
		safe_delete_column_headers();

		if ( nullptr == m_e_response.cxmla__return__.root.OlapInfo ) { return; }

		m_execute_colls = new ATLCOLUMNINFO[ m_e_response.cxmla__return__.root.OlapInfo->CellInfo.__cellProps.__size + 1 ];	
		DBLENGTH pos = 0;
		size_t crt = 0;
		m_cell_ordinal_pos = -1;

		if ( nullptr != m_e_response.cxmla__return__.root.OlapInfo->CellInfo.Value )
		{			
			m_execute_colls[crt].pwszName = _wcsdup( FROM_STRING( m_e_response.cxmla__return__.root.OlapInfo->CellInfo.Value->name, CP_UTF8 ) );
			m_execute_colls[crt].pTypeInfo = (ITypeInfo*)nullptr;
			m_execute_colls[crt].iOrdinal = crt + 1;
			m_execute_colls[crt].dwFlags = DBCOLUMNFLAGS_ISFIXEDLENGTH;
			m_execute_colls[crt].ulColumnSize = sizeof(VARIANT);
			m_execute_colls[crt].wType = DBTYPE_VARIANT;
			m_execute_colls[crt].bPrecision = 0;
			m_execute_colls[crt].bScale = 0;
			m_execute_colls[crt].cbOffset = pos;
			memset( &( m_execute_colls[crt].columnid ), 0, sizeof( DBID ));
	
			pos += m_execute_colls[crt].ulColumnSize;
			++crt;
			m_indirection.push_back( std::make_pair( std::string("Value"), 0 )  );
		}

		for ( int i = 0; i < m_e_response.cxmla__return__.root.OlapInfo->CellInfo.__cellProps.__size; ++i )
		{
			m_execute_colls[crt].pwszName = _wcsdup( FROM_STRING( m_e_response.cxmla__return__.root.OlapInfo->CellInfo.__cellProps.__array[i].name, CP_UTF8 ) );
			if ( 0 == wcscmp( m_execute_colls[crt].pwszName, L"CELL_ORDINAL" ) )
			{
				m_cell_ordinal_pos = crt;
			}
			m_execute_colls[crt].pTypeInfo = (ITypeInfo*)nullptr;
			m_execute_colls[crt].iOrdinal = crt + 1;
			m_execute_colls[crt].dwFlags = DBCOLUMNFLAGS_ISFIXEDLENGTH;
			m_execute_colls[crt].wType = dbtype_from_xsd( m_e_response.cxmla__return__.root.OlapInfo->CellInfo.__cellProps.__array[i].type );
			switch ( m_execute_colls[crt].wType )
			{
			case DBTYPE_WSTR:
				m_execute_colls[crt].dwFlags = DBCOLUMNFLAGS_MAYBENULL;
				m_execute_colls[crt].ulColumnSize = (ULONG)WORD_WIDTH; 
				m_execute_colls[crt].bPrecision = 0;
				m_execute_colls[crt].bScale = 0;
				m_execute_colls[crt].cbOffset = pos;
				pos += WORD_WIDTH * sizeof(wchar_t);
				break;
			case DBTYPE_UI4:
				m_execute_colls[crt].ulColumnSize = (ULONG)sizeof(unsigned int); 
				m_execute_colls[crt].bPrecision = (BYTE)0xFF; 
				m_execute_colls[crt].bScale = (BYTE)0xFF;
				m_execute_colls[crt].cbOffset = pos;
				pos += sizeof(unsigned int);
				break;
			case DBTYPE_I4:
				m_execute_colls[crt].ulColumnSize = (ULONG)sizeof(int); 
				m_execute_colls[crt].bPrecision = (BYTE)0xFF;
				m_execute_colls[crt].bScale = (BYTE)0xFF;
				m_execute_colls[crt].cbOffset = pos;
				pos += sizeof(int);
				break;
			case DBTYPE_UI2:
				m_execute_colls[crt].ulColumnSize = (ULONG)sizeof(unsigned short); 
				m_execute_colls[crt].bPrecision = (BYTE)0xFF; 
				m_execute_colls[crt].bScale = (BYTE)0xFF;
				m_execute_colls[crt].cbOffset = pos;
				pos += sizeof(unsigned short);
				break;
			case DBTYPE_I2:
				m_execute_colls[crt].ulColumnSize = (ULONG)sizeof(short); 
				m_execute_colls[crt].bPrecision = (BYTE)0xFF;
				m_execute_colls[crt].bScale = (BYTE)0xFF;
				m_execute_colls[crt].cbOffset = pos;
				pos += sizeof(short);
				break;
			}
			memset( &( m_execute_colls[crt].columnid ), 0, sizeof( DBID ));
			++crt;
			m_indirection.push_back( std::make_pair( std::string( m_e_response.cxmla__return__.root.OlapInfo->CellInfo.__cellProps.__array[i].elementName ), -1 )  );
		}
		m_execute_col_count = crt;
	}
public:
	connection_handler( IUnknown* aSession ) 
		: m_session( nullptr )
		, m_tab_data_access( nullptr )
	{
		IGetSelf* pGetSelf;
		aSession->QueryInterface( __uuidof( IGetSelf ) , ( void** ) &pGetSelf );
		pGetSelf->GetSelf( (void**)&m_session );
		pGetSelf->Release();

		m_session_id = session::session_table()[ m_session ].id;

		get_connection_data();

		m_proxy.header = NULL;

		if ( nullptr == fparsehdr ) 
		{
			fparsehdr = m_proxy.fparsehdr;
		}
		m_proxy.fparsehdr = http_post_parse_header;
		
		config_data::ssl_init( &m_proxy );
		
		config_data::get_proxy( m_location.c_str(), m_proxy.proxy_host, m_proxy.proxy_port );

		soap_2_session()[ &m_proxy ] = m_session;
		m_execute_colls = nullptr;
	}

	connection_handler( const std::string& location, const std::string& user, const std::string& pass, const std::string& catalog )
		: m_session( nullptr )
		, m_location(location)
		, m_user(user)
		, m_pass(pass)
		, m_catalog( catalog )
		, m_session_id( -1 )
		, m_tab_data_access( nullptr )
	{
		if ( nullptr == fparsehdr ) 
		{
			fparsehdr = m_proxy.fparsehdr;
		}
		m_proxy.fparsehdr = http_post_parse_header;
		
		config_data::ssl_init( &m_proxy );

		config_data::get_proxy( m_location.c_str(), m_proxy.proxy_host, m_proxy.proxy_port );

		m_execute_colls = nullptr;
	}

	virtual ~connection_handler()
	{
		soap_2_session().erase( &m_proxy );
		if ( nullptr != m_tab_data_access ) { delete m_tab_data_access; }
		safe_delete_column_headers();
	}

	void loadCubeDimProps( cxmla__DiscoverResponse&  aresponse ){
		xmlns__rows& rows = m_d_response.cxmla__return__.root.__rows;
		for( int i = 0; i < rows.__size; ++i ) {
			dim_properties::instance().addProperty( m_catalog, rows.row[i].CUBE_USCORENAME, rows.row[i].PROPERTY_USCORENAME, rows.row[i].HIERARCHY_USCOREUNIQUE_USCORENAME, rows.row[i].LEVEL_USCOREUNIQUE_USCORENAME ); 
			std::string alias = std::string(rows.row[i].HIERARCHY_USCOREUNIQUE_USCORENAME)+".["+rows.row[i].PROPERTY_USCORENAME+"]";
			std::string substr = std::string(rows.row[i].LEVEL_USCOREUNIQUE_USCORENAME)+".["+rows.row[i].PROPERTY_USCORENAME+"]";
			query_translator::translator().load_alias( alias, substr, session::session_table()[ m_session ].server);
		}
	}

	void loasAlias() {

	}

	int discover( char* endpoint, ULONG cRestrictions, const VARIANT* rgRestrictions)
	{
		m_proxy.soap_endpoint = m_location.c_str();

		soap_omode(&m_proxy, SOAP_XML_DEFAULTNS | SOAP_C_UTFSTRING);
		soap_imode(&m_proxy, SOAP_C_UTFSTRING);
		
		if ( -1 == m_session_id ) {
			begin_session();
			m_proxy.userid = m_user.c_str();
			m_proxy.passwd = m_pass.c_str();
		} else {
			//palo requires credentials inside the session
			m_proxy.userid = m_user.c_str();
			m_proxy.passwd = m_pass.c_str();
			session();
		}	

		bool loadProperties = false;
		xmlns__Restrictions restrictions;
		load_restrictions( cRestrictions, rgRestrictions, restrictions, strcmp("DISCOVER_LITERALS", endpoint) != 0 && strcmp("MDSCHEMA_FUNCTIONS", endpoint) != 0 );


		if ( ("MDSCHEMA_PROPERTIES" == endpoint) &&  ("MDSCHEMA_PROPERTIES" == endpoint) && (strcmp("1", restrictions.RestrictionList.PROPERTY_USCORETYPE) == 0) ) {
			loadProperties = true;
			if ( NULL != restrictions.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME ) {
				delete restrictions.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME;
				restrictions.RestrictionList.LEVEL_USCOREUNIQUE_USCORENAME = NULL;
			}
		}

		xmlns__Properties props;
		props.PropertyList.Catalog = const_cast<char*>(m_catalog.c_str());//make Palo happy	
		props.PropertyList.LocaleIdentifier = CP_UTF8;
		int result = m_proxy.Discover( endpoint, restrictions, props, m_d_response );

		if ( 0 == result && loadProperties ) {
			loadCubeDimProps( m_d_response );
		}

		if ( NULL != m_session && NULL != m_proxy.header && NULL != m_proxy.header->Session && NULL != m_proxy.header->Session->SessionId ) {
			session::session_table()[ m_session ].id = atoi( m_proxy.header->Session->SessionId );
		}
		unload_restrictions( restrictions );
		return result;
	}

	void get_cell_data( cxmla__ExecuteResponse&  aResponse ) 
	{
		if ( NULL == aResponse.cxmla__return__.root.CellData ) return;
		if ( NULL == aResponse.cxmla__return__.root.Axes ) return; 

		int size = 1;
		//get cellData size with empty values
		for( int i = 0; i < aResponse.cxmla__return__.root.Axes->__size; ++i ) {
			if (  strcmp (aResponse.cxmla__return__.root.Axes->Axis[i].name,"SlicerAxis") != 0) {
				size *= aResponse.cxmla__return__.root.Axes->Axis[i].Tuples.__size;
			}
		}

		m_cell_data.assign (size,-1);

		for( int i = 0; i < aResponse.cxmla__return__.root.CellData->__size; ++i ) {
			m_cell_data[ atoi(aResponse.cxmla__return__.root.CellData->Cell[i].CellOrdinal) ] = i;
		}
	}

	int execute ( char* statement )
	{
		bool tabular_result = false;
		m_proxy.soap_endpoint = m_location.c_str();
		soap_omode(&m_proxy, SOAP_XML_DEFAULTNS | SOAP_C_UTFSTRING);
		soap_imode(&m_proxy, SOAP_C_UTFSTRING);
		
		//soap_omode(&m_proxy,SOAP_XML_INDENT);
		if ( -1 == m_session_id ) {
			begin_session();
			m_proxy.userid = m_user.c_str();
			m_proxy.passwd = m_pass.c_str();
		} else {
			//palo requires credentials inside the session
			m_proxy.userid = m_user.c_str();
			m_proxy.passwd = m_pass.c_str();
			session();
		}	

		xmlns__Command command;

		std::string translation( statement );
		if ( nullptr !=  m_session )
		{
			query_translator::translator().translate( translation, session::session_table()[ m_session ].server );
		}
		statement = const_cast<char*>( translation.c_str() );

		command.Statement = statement;
		xmlns__Properties Properties;
		Properties.PropertyList.LocaleIdentifier = CP_UTF8;
		Properties.PropertyList.Content = "Data";
		Properties.PropertyList.AxisFormat = "TupleFormat";
		
		std::string drill_through_test(statement, statement+strlen("DRILLTHROUGH")); 
		std::transform( drill_through_test.begin(), drill_through_test.end(), drill_through_test.begin(), std::toupper );		
		
		if ( nullptr != m_tab_data_access  )
		{
			delete m_tab_data_access;
			m_tab_data_access = nullptr;
		}
		if ( drill_through_test == "DRILLTHROUGH" )
		{
			tabular_result = true;			
			Properties.PropertyList.Format = "Tabular";
		} else
		{
			Properties.PropertyList.Format = "Multidimensional";
		}

		Properties.PropertyList.Catalog = const_cast<char*>(m_catalog.c_str());
		//clear cell data
		m_cell_data.clear();
		int result = m_proxy.Execute( NULL, command, Properties, m_e_response );
		//add cell data
		get_cell_data( m_e_response );
		if ( NULL != m_session && NULL != m_proxy.header && NULL != m_proxy.header->Session && NULL != m_proxy.header->Session->SessionId ) {
			session::session_table()[ m_session ].id = atoi( m_proxy.header->Session->SessionId );
		}

		if ( tabular_result )
		{
			m_tab_data_access = new tabular_data_access( *this );
		}
		

		form_column_headers();

		return result;
	}

	const bool has_tabular_data() const 
	{ 
		return nullptr != m_tab_data_access; 
	}

	tabular_data_access& access_tab_data()
	{
		return *m_tab_data_access;
	}

	bool no_session() 
	{
		bool result = m_proxy.fault && 0 == strcmp(m_proxy.fault->faultstring,"Invalid Session id");
		if ( result ) {
			m_session_id = -1;
		}
		return result;
	}

	const cxmla__DiscoverResponse& discover_response() const
	{
		return m_d_response;
	}

	const cxmla__ExecuteResponse& execute_response() const
	{
		return m_e_response;
	}

	const char* fault_string()
	{
		if  ( NULL == m_proxy.fault ) {
			static const char* noInfo = "No further information.";
			return noInfo;
		}

		if ( nullptr != m_proxy.fault->detail && nullptr != m_proxy.fault->detail->error.desc )
		{
			return m_proxy.fault->detail->error.desc;
		}

		return m_proxy.fault->faultstring;
	}

	const bool valid_credentials()
	{
		if  ( NULL == m_proxy.fault ) { return true; }
		if  ( 401 ==  m_proxy.error ) { return false; }
		return ( NULL == strstr( m_proxy.fault->faultstring, "ORA-01005" )  && NULL == strstr( m_proxy.fault->faultstring, "ORA-01017" ) );
	}

	ATL::ATLCOLUMNINFO* column_info( DBORDINAL* pcInfo )
	{
		*pcInfo = m_execute_col_count;
		return m_execute_colls;
	}

	bool  is_cell_ordinal( size_t indirection )
	{
		return m_cell_ordinal_pos == indirection-1;
	}

	VARIANT at( DBORDINAL index, size_t indirection )
	{
		VARIANT result;
		result.vt = VT_NULL;

		if ( NULL == m_e_response.cxmla__return__.root.CellData ) {
			throw std::runtime_error( "no query response");
		}

		if ( index >= m_cell_data.size() ) {
			//empty response
			result.bstrVal = NULL;
			result.vt = VT_BSTR;
			return result;
		}

		if ( -1 == m_cell_data[index] ) {
			//empty response
			result.bstrVal = NULL;
			result.vt = VT_BSTR;
			return result;
		}

		//will only test once for user defined props

		if ( -1 == m_indirection[indirection-1].second )
		{
			m_indirection[indirection-1].second = -2;
			for ( int i = 0; i < m_e_response.cxmla__return__.root.CellData->Cell[m_cell_data[index]].__cellProps.__size; ++i )
			{
				if ( 0 == strcmp( m_indirection[indirection-1].first.c_str(), m_e_response.cxmla__return__.root.CellData->Cell[m_cell_data[index]].__cellProps.__array[i].elementName ) )
				{
					m_indirection[indirection-1].second = i+1;
					break;
				}
			}
		}

		switch ( m_indirection[indirection-1].second )
		{

		case 0://value
		{
			_Value& val = m_e_response.cxmla__return__.root.CellData->Cell[m_cell_data[index]].Value;

			if ( nullptr == val.__v )
			{
					result.bstrVal = NULL;
					result.vt = VT_BSTR;
					return result;
			}
			
			if ( 0 == strcmp( val.xsi__type, "xsd:double" ) ) {
				result.vt = VT_R8;
				if ( NULL == val.__v ) {
					result.bstrVal = NULL;
					result.vt = VT_BSTR;
				} else {
					char* end_pos;
					result.dblVal = strtod(val.__v,&end_pos);
					if ( 0 == result.dblVal && 0 != *end_pos )
					{
						result.bstrVal = SysAllocString( CA2W( val.__v, CP_UTF8 ) );
						result.vt = VT_BSTR;
					}
				}
			} else if ( 0 == strcmp( val.xsi__type, "xsd:string" ) ) {
				result.vt = VT_BSTR;
				if ( NULL != val.__v ) {
					result.bstrVal = SysAllocString( CA2W( val.__v, CP_UTF8 ) );
				} else {
					result.bstrVal = NULL;
				}
			} else if ( 0 == strcmp( val.xsi__type, "xsd:empty" ) ) {
				result.bstrVal = NULL;
				result.vt = VT_BSTR;
			} else if ( 0 == strcmp( val.xsi__type, "xsd:int" ) ) {
				result.vt = VT_I4;
				if ( NULL == val.__v ) {
					result.bstrVal = NULL;
					result.vt = VT_BSTR;
				} else {
					char* end_pos;
					result.intVal = strtol( val.__v, &end_pos, 10 );
					if ( 0 == result.intVal && 0 != *end_pos )
					{
						result.bstrVal = SysAllocString( CA2W( val.__v, CP_UTF8 ) );
						result.vt = VT_BSTR;
					}
				}
			} else if ( 0 == strcmp( val.xsi__type, "xsd:boolean" ) ) { 
				if ( NULL == val.__v ) {
					result.bstrVal = NULL;
					result.vt = VT_BSTR;
				} else {
					result.vt = VT_BOOL;
					result.boolVal = (std::strcmp("true", val.__v) == 0);
				}
			} else {
				//handle unknown as string
				result.vt = VT_BSTR;
				if ( NULL != val.__v ) {
					result.bstrVal = SysAllocString( CA2W( val.__v, CP_UTF8 ) );
				} else {
					result.bstrVal = NULL;
				}
			}
		}

		break;

		default:
		{			
			char* data = nullptr;
			if ( m_e_response.cxmla__return__.root.CellData->Cell[m_cell_data[index]].__cellProps.__size >= m_indirection[indirection-1].second )
			{
				data = const_cast<char*>( m_e_response.cxmla__return__.root.CellData->Cell[m_cell_data[index]].__cellProps.__array[m_indirection[indirection-1].second-1].value );
			}
			if ( data ) {
				DBTYPE data_type = m_execute_colls[indirection-1].wType;
				switch ( data_type )
				{
				case DBTYPE_WSTR:
					{
						result.vt = VT_BSTR;
						result.bstrVal = SysAllocString( CA2W( data, CP_UTF8 ) );
					}
					break;
				}
			} else {
				result.vt = VT_EMPTY;
			}	
		}
		break;

		case -2:
			result.vt = VT_EMPTY;
			break;

		}

		return result;
	}

	void get_axis_info( DBCOUNTITEM   *pcAxes, MDAXISINFO   **prgAxisInfo )
	{
		if ( nullptr == m_e_response.cxmla__return__.root.OlapInfo ) {
			throw std::runtime_error( "no query response");
		}

		if ( nullptr == m_e_response.cxmla__return__.root.OlapInfo ){
			throw std::runtime_error( "the server returned an invalid answer");
		}

		if ( nullptr == m_e_response.cxmla__return__.root.Axes ){
			throw std::runtime_error( "the server returned an invalid answer");
		}

		*pcAxes			= ( DBCOUNTITEM ) m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.__size;
		if ( 0 == *pcAxes ) {
			return;
		}

		//Mondrian gives an empty slicer
		DBCOUNTITEM idx = 0;

		MDAXISINFO* axisInfo = new MDAXISINFO[*pcAxes];
		for ( DBCOUNTITEM i = 0; i < *pcAxes; ++i ) {
			axisInfo[idx].cbSize = sizeof( MDAXISINFO );
			axisInfo[idx].rgcColumns = nullptr;
			axisInfo[idx].rgpwszDimensionNames = nullptr;
			axisInfo[idx].cCoordinates = m_e_response.cxmla__return__.root.Axes->Axis[idx].Tuples.__size;//count on the same order
			axisInfo[idx].cDimensions = m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[idx].__size;
			std::string name( m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[idx].name );
			std::transform( name.begin(), name.end(), name.begin(), std::tolower );
			if ( name.substr( 0, 4 ) == "axis" ) {
				axisInfo[idx].iAxis = atoi( name.substr(4, name.size() ).c_str() );
			} else {
				axisInfo[idx].iAxis = MDAXIS_SLICERS;
				if ( 0 == m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[idx].__size ) {
					//slicer was present but it was empty										
					axisInfo[idx].cCoordinates = 0;
					axisInfo[idx].cDimensions = 0;
					continue;
				}
			}
			axisInfo[idx].rgcColumns = new DBORDINAL[ axisInfo[idx].cDimensions ];
			axisInfo[idx].rgpwszDimensionNames = new LPOLESTR[ axisInfo[idx].cDimensions ];
			for ( DBCOUNTITEM j = 0; j < axisInfo[idx].cDimensions; ++j ) {
				xmlns__HierarchyInfo hInfo = m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[idx].HierarchyInfo[j];
				DBORDINAL col_count = 5;//required columns;
				if ( NULL != hInfo.PARENT_USCOREUNIQUE_USCORENAME ) {
					++col_count;
				}
				if ( NULL != hInfo.MEMBER_USCORENAME ) {
					++col_count;
				}
				if ( NULL != hInfo.MEMBER_USCORETYPE ) {
					++col_count;
				}
				col_count += hInfo.__userProp.__size;
				axisInfo[idx].rgcColumns[j] = col_count;
				axisInfo[idx].rgpwszDimensionNames[j] =  _wcsdup( CA2W( m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[idx].HierarchyInfo[j].name, CP_UTF8 ) );
			}
			++idx;
		}

		DWORD* leak = new DWORD[1];

		*prgAxisInfo	= axisInfo;
	}

	void free_axis_info( DBCOUNTITEM   cAxes, MDAXISINFO   *rgAxisInfo )
	{
		for ( DBCOUNTITEM i = 0; i < cAxes; ++i ) {
			if ( 0 == rgAxisInfo[i].cDimensions ) { continue; }
			delete[] rgAxisInfo[i].rgcColumns;
			for ( DBCOUNTITEM j = 0; j < rgAxisInfo[i].cDimensions; ++j ) {
				delete[] rgAxisInfo[i].rgpwszDimensionNames[j];
			}
			delete[] rgAxisInfo[i].rgpwszDimensionNames;
		}
		delete[] rgAxisInfo;
	}

	unsigned int row_count()
	{
		if ( NULL == m_e_response.cxmla__return__.root.CellData ) {
			return 0;
		}
		return m_e_response.cxmla__return__.root.CellData->__size;
	}

	void get_axis( DBCOUNTITEM idx, xmlns__Axis*& axisData, xmlns__AxisInfo*& axisInfo )
	{
		if ( NULL == m_e_response.cxmla__return__.root.Axes ) {
			throw std::runtime_error( "no query response");
		}

		if ( NULL == m_e_response.cxmla__return__.root.OlapInfo ) {
			throw std::runtime_error( "no query response");
		}

		std::string axisName;
		if ( MDAXIS_SLICERS == idx ) {
			axisName = "SlicerAxis";
		} else {
			std::stringstream buf;
			buf << "Axis";
			buf << idx;
			axisName = buf.str();
		}
		
		for ( int i = 0, e = m_e_response.cxmla__return__.root.Axes->__size; i < e; ++i ) {
			if ( axisName == m_e_response.cxmla__return__.root.Axes->Axis[i].name ) {
				axisData = &( m_e_response.cxmla__return__.root.Axes->Axis[i] );
			}
		}

		for ( DBCOUNTITEM i = 0, e = ( DBCOUNTITEM ) m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.__size; i < e; ++i ) {
			if ( axisName == m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[i].name ) {
				axisInfo = &(m_e_response.cxmla__return__.root.OlapInfo->AxesInfo.AxisInfo[i] );
			}
		}
	}
};
