/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
***************************************************************************

*/

#include "zEffector.h"
#include "tRandom.h"
#include "gGame.h"


void zEffector::apply(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    effect(d_calculatedTargets);
    sn_ConsoleOutMessage(message);

    if (count > 0) 
      count --;
  }
  
}

void zEffectorWin::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  // BOP
  static const char* message="$player_win_instant";
  // EOP

    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	sg_DeclareWinner((*iter)->CurrentTeam(), message );
      }
}

void zEffectorDeath::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	static_cast<gCycle *>((*iter)->Object())->Kill();
      }
    if (count > 0) 
      count --;
  }
}

//
//
//

void zEffectorPoint::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	(*iter)->AddScore(d_score, tOutput(), "$player_lose_suicide");
      }
    if (count > 0) 
      count --;
  }
}

//
//
//

void zEffectorCycleRubber::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	static_cast<gCycle *>((*iter)->Object())->SetRubber(0.0);
      }
    if (count > 0) 
      count --;
  }
}


//
//
//

void zEffectorCycleBrake::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	static_cast<gCycle *>((*iter)->Object())->SetBrakingReservoir(1.0);
      }
    if (count > 0) 
      count --;
  }
}

//
//
//

void zEffectorSpawnPlayer::effect(gVectorExtra<ePlayerNetID *> &d_calculatedTargets)
{
  if (count == -1 || count > 0) {
    gVectorExtra<ePlayerNetID *>::iterator iter;
    for(iter = d_calculatedTargets.begin();
	iter != d_calculatedTargets.end();
	++iter)
      {
	sg_RespawnPlayer(grid, arena, (*iter));
      }
    if (count > 0) 
      count --;
  }
}

