﻿/*
 *
 *  Copyright ( c ) 2011-2015
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "favorites.h"

#include "utility.h"
#include "json.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QCryptographicHash>

favorites::autoMount::autoMount() : m_autoMountVolume( false ),m_isSet( false )
{
}

favorites::autoMount::autoMount( bool e ) : m_autoMountVolume( e ),m_isSet( true )
{
}

bool favorites::autoMount::automount() const
{
	return m_autoMountVolume ;
}

void favorites::autoMount::toggle()
{
	if( m_isSet ){

		m_autoMountVolume = !m_autoMountVolume ;
	}
}

favorites::autoMount::operator bool() const
{
	return m_isSet ;
}

favorites::readOnly::readOnly() : m_isSet( false )
{
}

favorites::readOnly::readOnly( bool e ) : m_readOnlyVolume( e ),m_isSet( true )
{
}

bool favorites::readOnly::onlyRead() const
{
	return m_readOnlyVolume ;
}

favorites::readOnly::operator bool() const
{
	return m_isSet ;
}

static utility::result< QString > _config_path()
{
	QString m = settings::instance().ConfigLocation() + "/favorites/" ;

	if( utility::pathExists( m ) ){

		return m ;
	}else{
		if( QDir().mkdir( m ) ){

			return m ;
		}else{
			return {} ;
		}
	}
}

static QString _create_path( const QString& m,const favorites::entry& e )
{
	auto a = utility::split( e.volumePath,'/' ).last() ;
	auto b = a + e.mountPointPath ;
	auto c = QCryptographicHash::hash( b.toLatin1(),QCryptographicHash::Sha256 ) ;

	return m + a + "-" + c.toHex() + ".json" ;
}

static QString _create_path( const favorites::entry& e )
{
	auto s = _config_path() ;

	if( s.has_value() ){

		auto a = utility::split( e.volumePath,'/' ).last() ;
		auto b = a + e.mountPointPath ;
		auto c = QCryptographicHash::hash( b.toLatin1(),QCryptographicHash::Sha256 ) ;


		return s.value() + a + "-" + c.toHex() + ".json" ;
	}else{
		return {} ;
	}
}

static void _move_favorites_to_new_system( const QStringList& m )
{
	favorites::entry s ;

	QString autoMountVolume ;

	utility2::stringListToStrings( m,
				       s.volumePath,
				       s.mountPointPath,
				       autoMountVolume,
				       s.configFilePath,
				       s.idleTimeOut,
				       s.mountOptions ) ;

	if( autoMountVolume != "N/A" ){

		if( autoMountVolume == "true" ){

			s.autoMount = favorites::autoMount( true ) ;
		}else{
			s.autoMount = favorites::autoMount( false ) ;
		}
	}

	if( s.configFilePath == "N/A" ){

		s.configFilePath.clear() ;
	}

	if( s.idleTimeOut == "N/A" ){

		s.idleTimeOut.clear() ;
	}

	if( s.mountOptions == "N/A" ){

		s.mountOptions.clear() ;
	}

	s.reverseMode          = s.mountOptions.contains( "-SiriKaliReverseMode" ) ;
	s.volumeNeedNoPassword = s.mountOptions.contains( "-SiriKaliVolumeNeedNoPassword" ) ;

	if( s.mountOptions.contains( "-SiriKaliMountReadOnly" ) ){

		s.readOnlyMode = favorites::readOnly( true ) ;
	}

	s.mountOptions.replace( "-SiriKaliMountReadOnly","" ) ;
	s.mountOptions.replace( "-SiriKaliVolumeNeedNoPassword","" ) ;
	s.mountOptions.replace( "-SiriKaliReverseMode","" ) ;

	favorites::add( s ) ;
}

static void _add_entries( std::vector< favorites::entry >& e,const QString& path )
{
	try {
		auto json = nlohmann::json::parse( utility::fileContents( path ).toStdString() ) ;

		auto _getString = [ & ]( const char * s )->QString{

			return json[ s ].get< std::string >().c_str() ;
		} ;

		auto _getBool = [ & ]( const char * s ){

			return json[ s ].get< bool >() ;
		} ;

		favorites::entry m ;

		m.volumePath           = _getString( "volumePath" ) ;
		m.mountPointPath       = _getString( "mountPointPath" ) ;
		m.configFilePath       = _getString( "configFilePath" ) ;
		m.idleTimeOut          = _getString( "idleTimeOut" ) ;
		m.mountOptions         = _getString( "mountOptions" ) ;
		m.preMountCommand      = _getString( "preMountCommand" ) ;
		m.postMountCommand     = _getString( "postMountCommand" ) ;
		m.preUnmountCommand    = _getString( "preUnmountCommand" ) ;
		m.postUnmountCommand   = _getString( "postUnmountCommand" ) ;
		m.reverseMode          = _getBool( "reverseMode" ) ;
		m.volumeNeedNoPassword = _getBool( "volumeNeedNoPassword" ) ;

		auto s = _getString( "MountReadOnly" ) ;

		if( !s.isEmpty() ){

			m.readOnlyMode = favorites::readOnly( s == "true" ? true : false ) ;
		}

		s = _getString( "autoMountVolume" ) ;

		if( !s.isEmpty() ){

			m.autoMount = favorites::autoMount( s == "true" ? true : false ) ;
		}

		e.emplace_back( std::move( m ) ) ;

	}catch( ... ){

		utility::debug::cout() << "Failed to parse file for reading: " + path ;
	}
}

std::vector<favorites::entry> favorites::readFavorites()
{
	auto m = _config_path() ;

	if( !m.has_value() ){

		return {} ;
	}

	auto a = m.value() ;

	const auto s = QDir( a ).entryList( QDir::Filter::Files ) ;

	std::vector<favorites::entry> e ;

	for( const auto& it : s ){

		_add_entries( e,a + it ) ;
	}

	return e ;
}

favorites::entry favorites::readFavorite( const QString& e )
{
	for( const auto& it : favorites::readFavorites() ){

		if( it.volumePath == e ){

			return it ;
		}
	}

	return {} ;
}

void favorites::updateFavorites()
{
	auto& m = settings::instance().backend() ;

	if( m.contains( "FavoritesVolumes" ) ){

		const auto a = m.value( "FavoritesVolumes" ).toStringList() ;

		m.remove( "FavoritesVolumes" ) ;

		for( const auto& it : a ){

			_move_favorites_to_new_system( utility::split( it,'\t' ) ) ;
		}
	}
}

favorites::error favorites::add( const favorites::entry& e )
{
	auto m = _config_path() ;

	if( !m.has_value() ){

		return error::FAILED_TO_CREATE_ENTRY ;
	}

	nlohmann::json json ;

	json[ "volumePath" ]           = e.volumePath.toStdString() ;
	json[ "mountPointPath" ]       = e.mountPointPath.toStdString() ;
	json[ "configFilePath" ]       = e.configFilePath.toStdString() ;
	json[ "idleTimeOut" ]          = e.idleTimeOut.toStdString() ;
	json[ "mountOptions" ]         = e.mountOptions.toStdString() ;
	json[ "preMountCommand" ]      = e.preMountCommand.toStdString() ;
	json[ "postMountCommand" ]     = e.postMountCommand.toStdString() ;
	json[ "preUnmountCommand" ]    = e.preUnmountCommand.toStdString() ;
	json[ "postUnmountCommand" ]   = e.postUnmountCommand.toStdString() ;
	json[ "reverseMode" ]          = e.reverseMode ;
	json[ "volumeNeedNoPassword" ] = e.volumeNeedNoPassword ;


	if( e.readOnlyMode ){

		if( e.readOnlyMode.onlyRead() ){

			json[ "MountReadOnly" ] = "true" ;
		}else{
			json[ "MountReadOnly" ] = "false" ;
		}
	}else{
		json[ "MountReadOnly" ] = "" ;
	}

	if( e.autoMount ){

		if( e.autoMount.automount() ){

			json[ "autoMountVolume" ] = "true" ;
		}else{
			json[ "autoMountVolume" ] = "false" ;
		}
	}else{
		json[ "autoMountVolume" ] = "" ;
	}

	auto a = _create_path( m.value(),e ) ;

	if( utility::pathExists( a ) ){

		return error::ENTRY_ALREADY_EXISTS ;
	}else{
		QFile file( a ) ;

		if( file.open( QIODevice::WriteOnly ) ){

			file.write( json.dump( 8 ).data() ) ;

			return error::SUCCESS ;
		}else{
			return error::FAILED_TO_CREATE_ENTRY ;
		}
	}
}

void favorites::replaceFavorite( const favorites::entry& old,const favorites::entry& New )
{
	favorites::removeFavoriteEntry( old ) ;
	favorites::add( New ) ;
}

void favorites::removeFavoriteEntry( const favorites::entry& e )
{
	auto s = _create_path( e ) ;

	if( !s.isEmpty() ){

		QFile( s ).remove() ;
	}
}

favorites::entry::entry()
{

}

favorites::entry::entry( const QString& e )
{
	volumePath = e ;
}
