/*
        Thymio-II Firmware

        Copyright (C) 2011 Philippe Retornaz <philippe dot retornaz at epfl dot ch>,
        Mobots group (http://mobots.epfl.ch), Robotics system laboratory (http://lsro.epfl.ch)
        EPFL Ecole polytechnique federale de Lausanne (http://www.epfl.ch)

        See authors.txt for more details about other contributors.

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU Lesser General Public License as published
        by the Free Software Foundation, version 3 of the License.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU Lesser General Public License for more details.

        You should have received a copy of the GNU Lesser General Public License
        along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _PLAYBACK_H_
#define _PLAYBACK_H_

#define SOUND_DISABLE	(-1) /* Special case: immediatly stop playback */

#define SOUND_POWERON 	(-2)
#define SOUND_POWEROFF 	(-3)
#define SOUND_BUTTON	(-4)
#define SOUND_FREEFALL 	(-5)
#define SOUND_TAP	(-6)
#define SOUND_BUTTON_M	(-7)
#define SOUND_F_DETECT	(-8)
#define SOUND_F_OK	(-9)


// Negative: system sound
// Positive: user sound
void play_sound(int number);

void play_sound_loop(int number);

void play_sound_block(int number);

void play_frequency_block(int freq, int time);

#endif
