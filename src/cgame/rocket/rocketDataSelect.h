/*
===========================================================================

Unvanquished GPL Source Code
Copyright (C) 2012 Unvanquished Developers

This file is part of the Unvanquished GPL Source Code (Unvanquished Source Code).

Unvanquished Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Unvanquished Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Unvanquished Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Unvanquished Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Unvanquished
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

// Override for the default dataselect element.

#include <RmlUi/Core/Core.h>
#include <RmlUi/Controls/ElementFormControlDataSelect.h>

#include "../cg_local.h"

class RocketDataSelect : public Rml::Controls::ElementFormControlDataSelect, public Rml::Core::EventListener
{
public:
	RocketDataSelect( const Rml::Core::String &tag ) : Rml::Controls::ElementFormControlDataSelect( tag ), selection( -2 ) { }
	~RocketDataSelect() { }

	virtual void OnChildAdd( Element *child )
	{
		ElementFormControlDataSelect::OnChildAdd( child );

		if ( child == this )
		{
			owner = GetOwnerDocument();
			owner->AddEventListener( "show", this );
		}
	}

	virtual void OnChildRemove( Element *child )
	{
		ElementFormControlDataSelect::OnChildRemove( child );

		if (  child == this )
		{
			if ( owner )
			{
				owner->RemoveEventListener( "show", this );
			}
		}
	}

	virtual void OnAttributeChange( const Rml::Core::ElementAttributes &changed_attributes )
	{
		ElementFormControlDataSelect::OnAttributeChange( changed_attributes );
		auto it = changed_attributes.find( "source" );
		if ( it != changed_attributes.end() )
		{
			Rml::Core::String dataSource = it->second.Get<Rml::Core::String>();
			size_t pos = dataSource.find( "." );
			dsName = dataSource.substr( 0, pos );
			tableName =  dataSource.substr( pos + 1, dataSource.size() );
		}
	}

	virtual void ProcessEvent( Rml::Core::Event &event )
	{
		extern std::queue< RocketEvent_t * > eventQueue;

		if ( event.GetTargetElement() == owner && event == "show" )
		{
			eventQueue.push( new RocketEvent_t( this, va( "setDataSelectValue %s %s", dsName.c_str(), tableName.c_str() ) ) );
		}
	}

	void OnUpdate()
	{
		extern std::queue< RocketEvent_t * > eventQueue;

		ElementFormControlDataSelect::OnUpdate();

		if ( GetSelection() != selection )
		{
			selection = GetSelection();

			// dispatch event so cgame knows about it
			eventQueue.push( new RocketEvent_t( Rml::Core::String( va( "setDS %s %s %d", dsName.c_str(), tableName.c_str(), selection ) ) ) );

			// dispatch event so rocket knows about it
			Rml::Core::Dictionary parameters;
			parameters[ "index" ] = va( "%d", selection );
			parameters[ "datasource" ] = dsName;
			parameters[ "table" ] = tableName;
			DispatchEvent( "rowselect", parameters );
		}
	}
private:
	int selection;
	Rml::Core::Element *owner;
	Rml::Core::String dsName;
	Rml::Core::String tableName;
};

