/*
Copyright (C) 1994-1995 Apogee Software, Ltd.
Copyright (C) 2023 Frenkel Smeijers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**********************************************************************
   module: AL_MIDI.C

   author: James R. Dose
   date:   April 1, 1994

   Low level routines to support General MIDI music on Adlib compatible
   cards.

   (c) Copyright 1994 James R. Dose.  All Rights Reserved.
**********************************************************************/

#include <conio.h>
#include <dos.h>
#include <stddef.h>
#include <stdlib.h>
#include "interrup.h"
#include "sndcards.h"
#include "blaster.h"
#include "al_midi.h"


#define STEREO_DETUNE 5

#define lobyte( num )   ( ( unsigned )*( ( char * )&( num ) ) )
#define hibyte( num )   ( ( unsigned )*( ( ( char * )&( num ) ) + 1 ) )

#define AL_VoiceNotFound -1

#define  alFreqH     0xb0
#define  alEffects   0xbd

/* Number of slots for the voices on the chip */
#define NumChipSlots 18

#define NUM_VOICES      9
#define NUM_CHANNELS    16

#define NOTE_ON         0x2000  /* Used to turn note on or toggle note */
#define NOTE_OFF        0x0000

#define MAX_VELOCITY   0x7f
#define MAX_OCTAVE     7
#define MAX_NOTE       ( MAX_OCTAVE * 12 + 11 )
#define FINETUNE_MAX   31
#define FINETUNE_RANGE ( FINETUNE_MAX + 1 )

#define PITCHBEND_CENTER 1638400

#define note_off             0x80
#define note_on              0x90
#define poly_aftertouch      0xa0
#define control_change       0xb0
#define program_chng         0xc0
#define channel_aftertouch   0xd0
#define pitch_wheel          0xe0

#define MIDI_VOLUME          7
#define MIDI_PAN             10
#define MIDI_DETUNE          94
#define MIDI_ALL_NOTES_OFF   0x7B
#define MIDI_RESET_ALL_CONTROLLERS 0x79
#define MIDI_RPN_MSB               100
#define MIDI_RPN_LSB               101
#define MIDI_DATAENTRY_MSB         6
#define MIDI_DATAENTRY_LSB         38
#define MIDI_PITCHBEND_RPN         0

enum cromatic_scale
   {
   C       = 0x157,
   C_SHARP = 0x16B,
   D_FLAT  = 0x16B,
   D       = 0x181,
   D_SHARP = 0x198,
   E_FLAT  = 0x198,
   E       = 0x1B0,
   F_FLAT  = 0x1B0,
   E_SHARP = 0x1CA,
   F       = 0x1CA,
   F_SHARP = 0x1E5,
   G_FLAT  = 0x1E5,
   G       = 0x202,
   G_SHARP = 0x220,
   A_FLAT  = 0x220,
   A       = 0x241,
   A_SHARP = 0x263,
   B_FLAT  = 0x263,
   B       = 0x287,
   C_FLAT  = 0x287,
   B_SHARP = 0x2AE,
   };

/* Definition of octave information to be ORed onto F-Number */

enum octaves
   {
   OCTAVE_0 = 0x0000,
   OCTAVE_1 = 0x0400,
   OCTAVE_2 = 0x0800,
   OCTAVE_3 = 0x0C00,
   OCTAVE_4 = 0x1000,
   OCTAVE_5 = 0x1400,
   OCTAVE_6 = 0x1800,
   OCTAVE_7 = 0x1C00
   };

typedef struct VOICE
   {
   struct VOICE *next;
   struct VOICE *prev;

   unsigned num;
   unsigned key;
   unsigned velocity;
   unsigned channel;
   unsigned pitchleft;
   unsigned pitchright;
   int      timbre;
   int      port;
   unsigned status;
   } VOICE;

typedef struct
   {
   VOICE *start;
   VOICE *end;
   } VOICELIST;

typedef struct
   {
   VOICELIST Voices;
   int       Timbre;
   int       Pitchbend;
   int       KeyOffset;
   unsigned  KeyDetune;
   unsigned  Volume;
   unsigned  EffectiveVolume;
   int       Pan;
   int       Detune;
   unsigned  RPN;
   short     PitchBendRange;
   short     PitchBendSemiTones;
   short     PitchBendHundreds;
   } CHANNEL;

typedef struct
   {
   unsigned char SAVEK[ 2 ];
   unsigned char Level[ 2 ];
   unsigned char Env1[ 2 ];
   unsigned char Env2[ 2 ];
   unsigned char Wave[ 2 ];
   unsigned char Feedback;
   signed   char Transpose;
   signed   char Velocity;
   } TIMBRE;

//extern TIMBRE ADLIB_TimbreBank[ 256 ];
static TIMBRE ADLIB_TimbreBank[ 256 ] =
   {
      { { 33, 33 }, { 143, 6 }, { 242, 242 }, { 69, 118 }, { 0, 0 }, 8, 0 },
      { { 49, 33 }, { 75, 0 }, { 242, 242 }, { 84, 86 }, { 0, 0 }, 8, 0 },
      { { 49, 33 }, { 73, 0 }, { 242, 242 }, { 85, 118 }, { 0, 0 }, 8, 0 },
      { { 177, 97 }, { 14, 0 }, { 242, 243 }, { 59, 11 }, { 0, 0 }, 6, 0 },
      { { 1, 33 }, { 87, 0 }, { 241, 241 }, { 56, 40 }, { 0, 0 }, 0, 0 },
      { { 1, 33 }, { 147, 0 }, { 241, 241 }, { 56, 40 }, { 0, 0 }, 0, 0 },
      { { 33, 54 }, { 128, 14 }, { 162, 241 }, { 1, 213 }, { 0, 0 }, 8, 0 },
      { { 1, 1 }, { 146, 0 }, { 194, 194 }, { 168, 88 }, { 0, 0 }, 10, 0 },
      { { 12, 129 }, { 92, 0 }, { 246, 243 }, { 84, 181 }, { 0, 0 }, 0, 0 },
      { { 7, 17 }, { 151, 128 }, { 246, 245 }, { 50, 17 }, { 0, 0 }, 2, 0 },
      { { 23, 1 }, { 33, 0 }, { 86, 246 }, { 4, 4 }, { 0, 0 }, 2, 0 },
      { { 24, 129 }, { 98, 0 }, { 243, 242 }, { 230, 246 }, { 0, 0 }, 0, 0 },
      { { 24, 33 }, { 35, 0 }, { 247, 229 }, { 85, 216 }, { 0, 0 }, 0, 0 },
      { { 21, 1 }, { 145, 0 }, { 246, 246 }, { 166, 230 }, { 0, 0 }, 4, 0 },
      { { 69, 129 }, { 89, 128 }, { 211, 163 }, { 130, 227 }, { 0, 0 }, 12, 0 },
      { { 3, 129 }, { 73, 128 }, { 116, 179 }, { 85, 5 }, { 1, 0 }, 4, 0 },
      { { 113, 49 }, { 146, 0 }, { 246, 241 }, { 20, 7 }, { 0, 0 }, 2, 0 },
      { { 114, 48 }, { 20, 0 }, { 199, 199 }, { 88, 8 }, { 0, 0 }, 2, 0 },
      { { 112, 177 }, { 68, 0 }, { 170, 138 }, { 24, 8 }, { 0, 0 }, 4, 0 },
      { { 35, 177 }, { 147, 0 }, { 151, 85 }, { 35, 20 }, { 1, 0 }, 4, 0 },
      { { 97, 177 }, { 19, 128 }, { 151, 85 }, { 4, 4 }, { 1, 0 }, 0, 0 },
      { { 36, 177 }, { 72, 0 }, { 152, 70 }, { 42, 26 }, { 1, 0 }, 12, 0 },
      { { 97, 33 }, { 19, 0 }, { 145, 97 }, { 6, 7 }, { 1, 0 }, 10, 0 },
      { { 33, 161 }, { 19, 137 }, { 113, 97 }, { 6, 7 }, { 0, 0 }, 6, 0 },
      { { 2, 65 }, { 156, 128 }, { 243, 243 }, { 148, 200 }, { 1, 0 }, 12, 0 },
      { { 3, 17 }, { 84, 0 }, { 243, 241 }, { 154, 231 }, { 1, 0 }, 12, 0 },
      { { 35, 33 }, { 95, 0 }, { 241, 242 }, { 58, 248 }, { 0, 0 }, 0, 0 },
      { { 3, 33 }, { 135, 128 }, { 246, 243 }, { 34, 243 }, { 1, 0 }, 6, 0 },
      { { 3, 33 }, { 71, 0 }, { 249, 246 }, { 84, 58 }, { 0, 0 }, 0, 0 },
      { { 35, 33 }, { 72, 0 }, { 149, 132 }, { 25, 25 }, { 1, 0 }, 8, 0 },
      { { 35, 33 }, { 74, 0 }, { 149, 148 }, { 25, 25 }, { 1, 0 }, 8, 0 },
      { { 9, 132 }, { 161, 128 }, { 32, 209 }, { 79, 248 }, { 0, 0 }, 8, 0 },
      { { 33, 162 }, { 30, 0 }, { 148, 195 }, { 6, 166 }, { 0, 0 }, 2, 0 },
      { { 49, 49 }, { 18, 0 }, { 241, 241 }, { 40, 24 }, { 0, 0 }, 10, 0 },
      { { 49, 49 }, { 141, 0 }, { 241, 241 }, { 232, 120 }, { 0, 0 }, 10, 0 },
      { { 49, 50 }, { 91, 0 }, { 81, 113 }, { 40, 72 }, { 0, 0 }, 12, 0 },
      { { 1, 33 }, { 139, 64 }, { 161, 242 }, { 154, 223 }, { 0, 0 }, 8, 0 },
      { { 1, 33 }, { 137, 64 }, { 161, 242 }, { 154, 223 }, { 0, 0 }, 8, 0 },
      { { 49, 49 }, { 139, 0 }, { 244, 241 }, { 232, 120 }, { 0, 0 }, 10, 0 },
      { { 49, 49 }, { 18, 0 }, { 241, 241 }, { 40, 24 }, { 0, 0 }, 10, 0 },
      { { 49, 33 }, { 21, 0 }, { 221, 86 }, { 19, 38 }, { 1, 0 }, 8, 0 },
      { { 49, 33 }, { 22, 0 }, { 221, 102 }, { 19, 6 }, { 1, 0 }, 8, 0 },
      { { 113, 49 }, { 73, 0 }, { 209, 97 }, { 28, 12 }, { 1, 0 }, 8, 0 },
      { { 33, 35 }, { 77, 128 }, { 113, 114 }, { 18, 6 }, { 1, 0 }, 2, 0 },
      { { 241, 225 }, { 64, 0 }, { 241, 111 }, { 33, 22 }, { 1, 0 }, 2, 0 },
      { { 2, 1 }, { 26, 128 }, { 245, 133 }, { 117, 53 }, { 1, 0 }, 0, 0 },
      { { 2, 1 }, { 29, 128 }, { 245, 243 }, { 117, 244 }, { 1, 0 }, 0, 0 },
      { { 16, 17 }, { 65, 0 }, { 245, 242 }, { 5, 195 }, { 1, 0 }, 2, 0 },
      { { 33, 162 }, { 155, 1 }, { 177, 114 }, { 37, 8 }, { 1, 0 }, 14, 0 },
      { { 161, 33 }, { 152, 0 }, { 127, 63 }, { 3, 7 }, { 1, 1 }, 0, 0 },
      { { 161, 97 }, { 147, 0 }, { 193, 79 }, { 18, 5 }, { 0, 0 }, 10, 0 },
      { { 33, 97 }, { 24, 0 }, { 193, 79 }, { 34, 5 }, { 0, 0 }, 12, 0 },
      { { 49, 114 }, { 91, 131 }, { 244, 138 }, { 21, 5 }, { 0, 0 }, 0, 0 },
      { { 161, 97 }, { 144, 0 }, { 116, 113 }, { 57, 103 }, { 0, 0 }, 0, 0 },
      { { 113, 114 }, { 87, 0 }, { 84, 122 }, { 5, 5 }, { 0, 0 }, 12, 0 },
      { { 144, 65 }, { 0, 0 }, { 84, 165 }, { 99, 69 }, { 0, 0 }, 8, 0 },
      { { 33, 33 }, { 146, 1 }, { 133, 143 }, { 23, 9 }, { 0, 0 }, 12, 0 },
      { { 33, 33 }, { 148, 5 }, { 117, 143 }, { 23, 9 }, { 0, 0 }, 12, 0 },
      { { 33, 97 }, { 148, 0 }, { 118, 130 }, { 21, 55 }, { 0, 0 }, 12, 0 },
      { { 49, 33 }, { 67, 0 }, { 158, 98 }, { 23, 44 }, { 1, 1 }, 2, 0 },
      { { 33, 33 }, { 155, 0 }, { 97, 127 }, { 106, 10 }, { 0, 0 }, 2, 0 },
      { { 97, 34 }, { 138, 6 }, { 117, 116 }, { 31, 15 }, { 0, 0 }, 8, 0 },
      { { 161, 33 }, { 134, 13 }, { 114, 113 }, { 85, 24 }, { 1, 0 }, 0, 0 },
      { { 33, 33 }, { 77, 0 }, { 84, 166 }, { 60, 28 }, { 0, 0 }, 8, 0 },
      { { 49, 97 }, { 143, 0 }, { 147, 114 }, { 2, 11 }, { 1, 0 }, 8, 0 },
      { { 49, 97 }, { 142, 0 }, { 147, 114 }, { 3, 9 }, { 1, 0 }, 8, 0 },
      { { 49, 97 }, { 145, 0 }, { 147, 130 }, { 3, 9 }, { 1, 0 }, 10, 0 },
      { { 49, 97 }, { 142, 0 }, { 147, 114 }, { 15, 15 }, { 1, 0 }, 10, 0 },
      { { 33, 33 }, { 75, 0 }, { 170, 143 }, { 22, 10 }, { 1, 0 }, 8, 0 },
      { { 49, 33 }, { 144, 0 }, { 126, 139 }, { 23, 12 }, { 1, 1 }, 6, 0 },
      { { 49, 50 }, { 129, 0 }, { 117, 97 }, { 25, 25 }, { 1, 0 }, 0, 0 },
      { { 50, 33 }, { 144, 0 }, { 155, 114 }, { 33, 23 }, { 0, 0 }, 4, 0 },
      { { 225, 225 }, { 31, 0 }, { 133, 101 }, { 95, 26 }, { 0, 0 }, 0, 0 },
      { { 225, 225 }, { 70, 0 }, { 136, 101 }, { 95, 26 }, { 0, 0 }, 0, 0 },
      { { 161, 33 }, { 156, 0 }, { 117, 117 }, { 31, 10 }, { 0, 0 }, 2, 0 },
      { { 49, 33 }, { 139, 0 }, { 132, 101 }, { 88, 26 }, { 0, 0 }, 0, 0 },
      { { 225, 161 }, { 76, 0 }, { 102, 101 }, { 86, 38 }, { 0, 0 }, 0, 0 },
      { { 98, 161 }, { 203, 0 }, { 118, 85 }, { 70, 54 }, { 0, 0 }, 0, 0 },
      { { 98, 161 }, { 153, 0 }, { 87, 86 }, { 7, 7 }, { 0, 0 }, 11, 0 },
      { { 98, 161 }, { 147, 0 }, { 119, 118 }, { 7, 7 }, { 0, 0 }, 11, 0 },
      { { 34, 33 }, { 89, 0 }, { 255, 255 }, { 3, 15 }, { 2, 0 }, 0, 0 },
      { { 33, 33 }, { 14, 0 }, { 255, 255 }, { 15, 15 }, { 1, 1 }, 0, 0 },
      { { 34, 33 }, { 70, 128 }, { 134, 100 }, { 85, 24 }, { 0, 0 }, 0, 0 },
      { { 33, 161 }, { 69, 0 }, { 102, 150 }, { 18, 10 }, { 0, 0 }, 0, 0 },
      { { 33, 34 }, { 139, 0 }, { 146, 145 }, { 42, 42 }, { 1, 0 }, 0, 0 },
      { { 162, 97 }, { 158, 64 }, { 223, 111 }, { 5, 7 }, { 0, 0 }, 2, 0 },
      { { 32, 96 }, { 26, 0 }, { 239, 143 }, { 1, 6 }, { 0, 2 }, 0, 0 },
      { { 33, 33 }, { 143, 128 }, { 241, 244 }, { 41, 9 }, { 0, 0 }, 10, 0 },
      { { 119, 161 }, { 165, 0 }, { 83, 160 }, { 148, 5 }, { 0, 0 }, 2, 0 },
      { { 97, 177 }, { 31, 128 }, { 168, 37 }, { 17, 3 }, { 0, 0 }, 10, 0 },
      { { 97, 97 }, { 23, 0 }, { 145, 85 }, { 52, 22 }, { 0, 0 }, 12, 0 },
      { { 113, 114 }, { 93, 0 }, { 84, 106 }, { 1, 3 }, { 0, 0 }, 0, 0 },
      { { 33, 162 }, { 151, 0 }, { 33, 66 }, { 67, 53 }, { 0, 0 }, 8, 0 },
      { { 161, 33 }, { 28, 0 }, { 161, 49 }, { 119, 71 }, { 1, 1 }, 0, 0 },
      { { 33, 97 }, { 137, 3 }, { 17, 66 }, { 51, 37 }, { 0, 0 }, 10, 0 },
      { { 161, 33 }, { 21, 0 }, { 17, 207 }, { 71, 7 }, { 1, 0 }, 0, 0 },
      { { 58, 81 }, { 206, 0 }, { 248, 134 }, { 246, 2 }, { 0, 0 }, 2, 0 },
      { { 33, 33 }, { 21, 0 }, { 33, 65 }, { 35, 19 }, { 1, 0 }, 0, 0 },
      { { 6, 1 }, { 91, 0 }, { 116, 165 }, { 149, 114 }, { 0, 0 }, 0, 0 },
      { { 34, 97 }, { 146, 131 }, { 177, 242 }, { 129, 38 }, { 0, 0 }, 12, 0 },
      { { 65, 66 }, { 77, 0 }, { 241, 242 }, { 81, 245 }, { 1, 0 }, 0, 0 },
      { { 97, 163 }, { 148, 128 }, { 17, 17 }, { 81, 19 }, { 1, 0 }, 6, 0 },
      { { 97, 161 }, { 140, 128 }, { 17, 29 }, { 49, 3 }, { 0, 0 }, 6, 0 },
      { { 164, 97 }, { 76, 0 }, { 243, 129 }, { 115, 35 }, { 1, 0 }, 4, 0 },
      { { 2, 7 }, { 133, 3 }, { 210, 242 }, { 83, 246 }, { 0, 1 }, 0, 0 },
      { { 17, 19 }, { 12, 128 }, { 163, 162 }, { 17, 229 }, { 1, 0 }, 0, 0 },
      { { 17, 17 }, { 6, 0 }, { 246, 242 }, { 65, 230 }, { 1, 2 }, 4, 0 },
      { { 147, 145 }, { 145, 0 }, { 212, 235 }, { 50, 17 }, { 0, 1 }, 8, 0 },
      { { 4, 1 }, { 79, 0 }, { 250, 194 }, { 86, 5 }, { 0, 0 }, 12, 0 },
      { { 33, 34 }, { 73, 0 }, { 124, 111 }, { 32, 12 }, { 0, 1 }, 6, 0 },
      { { 49, 33 }, { 133, 0 }, { 221, 86 }, { 51, 22 }, { 1, 0 }, 10, 0 },
      { { 32, 33 }, { 4, 129 }, { 218, 143 }, { 5, 11 }, { 2, 0 }, 6, 0 },
      { { 5, 3 }, { 106, 128 }, { 241, 195 }, { 229, 229 }, { 0, 0 }, 6, 0 },
      { { 7, 2 }, { 21, 0 }, { 236, 248 }, { 38, 22 }, { 0, 0 }, 10, 0 },
      { { 5, 1 }, { 157, 0 }, { 103, 223 }, { 53, 5 }, { 0, 0 }, 8, 0 },
      { { 24, 18 }, { 150, 0 }, { 250, 248 }, { 40, 229 }, { 0, 0 }, 10, 0 },
      { { 16, 0 }, { 134, 3 }, { 168, 250 }, { 7, 3 }, { 0, 0 }, 6, 0 },
      { { 17, 16 }, { 65, 3 }, { 248, 243 }, { 71, 3 }, { 2, 0 }, 4, 0 },
      { { 1, 16 }, { 142, 0 }, { 241, 243 }, { 6, 2 }, { 2, 0 }, 14, 0 },
      { { 14, 192 }, { 0, 0 }, { 31, 31 }, { 0, 255 }, { 0, 3 }, 14, 0 },
      { { 6, 3 }, { 128, 136 }, { 248, 86 }, { 36, 132 }, { 0, 2 }, 14, 0 },
      { { 14, 208 }, { 0, 5 }, { 248, 52 }, { 0, 4 }, { 0, 3 }, 14, 0 },
      { { 14, 192 }, { 0, 0 }, { 246, 31 }, { 0, 2 }, { 0, 3 }, 14, 0 },
      { { 213, 218 }, { 149, 64 }, { 55, 86 }, { 163, 55 }, { 0, 0 }, 0, 0 },
      { { 53, 20 }, { 92, 8 }, { 178, 244 }, { 97, 21 }, { 2, 0 }, 10, 0 },
      { { 14, 208 }, { 0, 0 }, { 246, 79 }, { 0, 245 }, { 0, 3 }, 14, 0 },
      { { 38, 228 }, { 0, 0 }, { 255, 18 }, { 1, 22 }, { 0, 1 }, 14, 0 },
      { { 0, 0 }, { 0, 0 }, { 243, 246 }, { 240, 201 }, { 0, 2 }, 14, 0 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 0, 0 }, { 0, 0 }, { 252, 250 }, { 5, 23 }, { 2, 0 }, 14, 52 },
      { { 0, 1 }, { 2, 0 }, { 255, 255 }, { 7, 8 }, { 0, 0 }, 0, 48 },
      { { 0, 0 }, { 0, 0 }, { 252, 250 }, { 5, 23 }, { 2, 0 }, 14, 58 },
      { { 0, 0 }, { 0, 0 }, { 246, 246 }, { 12, 6 }, { 0, 0 }, 4, 60 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 47 },
      { { 0, 0 }, { 3, 0 }, { 248, 246 }, { 42, 69 }, { 0, 1 }, 4, 43 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 49 },
      { { 0, 0 }, { 3, 0 }, { 248, 246 }, { 42, 69 }, { 0, 1 }, 4, 43 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 51 },
      { { 0, 0 }, { 3, 0 }, { 248, 246 }, { 42, 69 }, { 0, 1 }, 4, 43 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 54 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 57 },
      { { 0, 0 }, { 3, 0 }, { 248, 246 }, { 42, 69 }, { 0, 1 }, 4, 72 },
      { { 12, 18 }, { 0, 0 }, { 246, 251 }, { 8, 71 }, { 0, 2 }, 10, 60 },
      { { 14, 208 }, { 0, 10 }, { 245, 159 }, { 48, 2 }, { 0, 0 }, 14, 76 },
      { { 14, 7 }, { 10, 93 }, { 228, 245 }, { 228, 229 }, { 3, 1 }, 6, 84 },
      { { 2, 5 }, { 3, 10 }, { 180, 151 }, { 4, 247 }, { 0, 0 }, 14, 36 },
      { { 78, 158 }, { 0, 0 }, { 246, 159 }, { 0, 2 }, { 0, 3 }, 14, 76 },
      { { 17, 16 }, { 69, 8 }, { 248, 243 }, { 55, 5 }, { 2, 0 }, 8, 84 },
      { { 14, 208 }, { 0, 0 }, { 246, 159 }, { 0, 2 }, { 0, 3 }, 14, 83 },
      { { 128, 16 }, { 0, 13 }, { 255, 255 }, { 3, 20 }, { 3, 0 }, 12, 84 },
      { { 14, 7 }, { 8, 81 }, { 248, 244 }, { 66, 228 }, { 0, 3 }, 14, 24 },
      { { 14, 208 }, { 0, 10 }, { 245, 159 }, { 48, 2 }, { 0, 0 }, 14, 77 },
      { { 1, 2 }, { 0, 0 }, { 250, 200 }, { 191, 151 }, { 0, 0 }, 7, 60 },
      { { 1, 1 }, { 81, 0 }, { 250, 250 }, { 135, 183 }, { 0, 0 }, 6, 65 },
      { { 1, 2 }, { 84, 0 }, { 250, 248 }, { 141, 184 }, { 0, 0 }, 6, 59 },
      { { 1, 2 }, { 89, 0 }, { 250, 248 }, { 136, 182 }, { 0, 0 }, 6, 51 },
      { { 1, 0 }, { 0, 0 }, { 249, 250 }, { 10, 6 }, { 3, 0 }, 14, 45 },
      { { 0, 0 }, { 128, 0 }, { 249, 246 }, { 137, 108 }, { 3, 0 }, 14, 71 },
      { { 3, 12 }, { 128, 8 }, { 248, 246 }, { 136, 182 }, { 3, 0 }, 15, 60 },
      { { 3, 12 }, { 133, 0 }, { 248, 246 }, { 136, 182 }, { 3, 0 }, 15, 58 },
      { { 14, 0 }, { 64, 8 }, { 118, 119 }, { 79, 24 }, { 0, 2 }, 14, 53 },
      { { 14, 3 }, { 64, 0 }, { 200, 155 }, { 73, 105 }, { 0, 2 }, 14, 64 },
      { { 215, 199 }, { 220, 0 }, { 173, 141 }, { 5, 5 }, { 3, 0 }, 14, 71 },
      { { 215, 199 }, { 220, 0 }, { 168, 136 }, { 4, 4 }, { 3, 0 }, 14, 61 },
      { { 128, 17 }, { 0, 0 }, { 246, 103 }, { 6, 23 }, { 3, 3 }, 14, 61 },
      { { 128, 17 }, { 0, 9 }, { 245, 70 }, { 5, 22 }, { 2, 3 }, 14, 48 },
      { { 6, 21 }, { 63, 0 }, { 0, 247 }, { 244, 245 }, { 0, 0 }, 1, 48 },
      { { 6, 18 }, { 63, 0 }, { 0, 247 }, { 244, 245 }, { 3, 0 }, 0, 69 },
      { { 6, 18 }, { 63, 0 }, { 0, 247 }, { 244, 245 }, { 0, 0 }, 1, 68 },
      { { 1, 2 }, { 88, 0 }, { 103, 117 }, { 231, 7 }, { 0, 0 }, 0, 63 },
      { { 65, 66 }, { 69, 8 }, { 248, 117 }, { 72, 5 }, { 0, 0 }, 0, 74 },
      { { 10, 30 }, { 64, 78 }, { 224, 255 }, { 240, 5 }, { 3, 0 }, 8, 60 },
      { { 10, 30 }, { 124, 82 }, { 224, 255 }, { 240, 2 }, { 3, 0 }, 8, 80 },
      { { 14, 0 }, { 64, 8 }, { 122, 123 }, { 74, 27 }, { 0, 2 }, 14, 64 },
      { { 14, 7 }, { 10, 64 }, { 228, 85 }, { 228, 57 }, { 3, 1 }, 6, 69 },
      { { 5, 4 }, { 5, 64 }, { 249, 214 }, { 50, 165 }, { 3, 0 }, 14, 73 },
      { { 2, 21 }, { 63, 0 }, { 0, 247 }, { 243, 245 }, { 3, 0 }, 8, 75 },
      { { 1, 2 }, { 79, 0 }, { 250, 248 }, { 141, 181 }, { 0, 0 }, 7, 68 },
      { { 0, 0 }, { 0, 0 }, { 246, 246 }, { 12, 6 }, { 0, 0 }, 4, 48 },
      { { 33, 17 }, { 17, 0 }, { 163, 196 }, { 67, 34 }, { 2, 0 }, 13, 53 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 },
      { { 16, 17 }, { 68, 0 }, { 248, 243 }, { 119, 6 }, { 2, 0 }, 8, 35 }
   };

static void AL_ResetVoices( void );
static void AL_CalcPitchInfo( void );
static void AL_SetVoiceTimbre( int voice );
static void AL_SetVoiceVolume( int voice );
static int  AL_AllocVoice( void );
static int  AL_GetVoice( int channel, int key );
static void AL_SetVoicePitch( int voice );
static void AL_SetChannelVolume( int channel, int volume );
static void AL_SetChannelPan( int channel, int pan );
static void AL_SetChannelDetune( int channel, int detune );


#define TRUE  ( 1 == 1 )
#define FALSE ( !TRUE )

static unsigned OctavePitch[ MAX_OCTAVE + 1 ] =
   {
   OCTAVE_0, OCTAVE_1, OCTAVE_2, OCTAVE_3,
   OCTAVE_4, OCTAVE_5, OCTAVE_6, OCTAVE_7,
   };

static unsigned NoteMod12[ MAX_NOTE + 1 ];
static unsigned NoteDiv12[ MAX_NOTE + 1 ];

// Pitch table

//static unsigned NotePitch[ FINETUNE_MAX + 1 ][ 12 ] =
//   {
//      { C, C_SHARP, D, D_SHARP, E, F, F_SHARP, G, G_SHARP, A, A_SHARP, B },
//   };

static unsigned NotePitch[ FINETUNE_MAX + 1 ][ 12 ] =
   {
      { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287 },
      { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x242, 0x264, 0x288 },
      { 0x158, 0x16c, 0x182, 0x199, 0x1b1, 0x1cb, 0x1e6, 0x203, 0x221, 0x243, 0x265, 0x289 },
      { 0x158, 0x16c, 0x183, 0x19a, 0x1b2, 0x1cc, 0x1e7, 0x204, 0x222, 0x244, 0x266, 0x28a },
      { 0x159, 0x16d, 0x183, 0x19a, 0x1b3, 0x1cd, 0x1e8, 0x205, 0x223, 0x245, 0x267, 0x28b },
      { 0x15a, 0x16e, 0x184, 0x19b, 0x1b3, 0x1ce, 0x1e9, 0x206, 0x224, 0x246, 0x268, 0x28c },
      { 0x15a, 0x16e, 0x185, 0x19c, 0x1b4, 0x1ce, 0x1ea, 0x207, 0x225, 0x247, 0x269, 0x28e },
      { 0x15b, 0x16f, 0x185, 0x19d, 0x1b5, 0x1cf, 0x1eb, 0x208, 0x226, 0x248, 0x26a, 0x28f },
      { 0x15b, 0x170, 0x186, 0x19d, 0x1b6, 0x1d0, 0x1ec, 0x209, 0x227, 0x249, 0x26b, 0x290 },
      { 0x15c, 0x170, 0x187, 0x19e, 0x1b7, 0x1d1, 0x1ec, 0x20a, 0x228, 0x24a, 0x26d, 0x291 },
      { 0x15d, 0x171, 0x188, 0x19f, 0x1b7, 0x1d2, 0x1ed, 0x20b, 0x229, 0x24b, 0x26e, 0x292 },
      { 0x15d, 0x172, 0x188, 0x1a0, 0x1b8, 0x1d3, 0x1ee, 0x20c, 0x22a, 0x24c, 0x26f, 0x293 },
      { 0x15e, 0x172, 0x189, 0x1a0, 0x1b9, 0x1d4, 0x1ef, 0x20d, 0x22b, 0x24d, 0x270, 0x295 },
      { 0x15f, 0x173, 0x18a, 0x1a1, 0x1ba, 0x1d4, 0x1f0, 0x20e, 0x22c, 0x24e, 0x271, 0x296 },
      { 0x15f, 0x174, 0x18a, 0x1a2, 0x1bb, 0x1d5, 0x1f1, 0x20f, 0x22d, 0x24f, 0x272, 0x297 },
      { 0x160, 0x174, 0x18b, 0x1a3, 0x1bb, 0x1d6, 0x1f2, 0x210, 0x22e, 0x250, 0x273, 0x298 },
      { 0x161, 0x175, 0x18c, 0x1a3, 0x1bc, 0x1d7, 0x1f3, 0x211, 0x22f, 0x251, 0x274, 0x299 },
      { 0x161, 0x176, 0x18c, 0x1a4, 0x1bd, 0x1d8, 0x1f4, 0x212, 0x230, 0x252, 0x276, 0x29b },
      { 0x162, 0x176, 0x18d, 0x1a5, 0x1be, 0x1d9, 0x1f5, 0x212, 0x231, 0x254, 0x277, 0x29c },
      { 0x162, 0x177, 0x18e, 0x1a6, 0x1bf, 0x1d9, 0x1f5, 0x213, 0x232, 0x255, 0x278, 0x29d },
      { 0x163, 0x178, 0x18f, 0x1a6, 0x1bf, 0x1da, 0x1f6, 0x214, 0x233, 0x256, 0x279, 0x29e },
      { 0x164, 0x179, 0x18f, 0x1a7, 0x1c0, 0x1db, 0x1f7, 0x215, 0x235, 0x257, 0x27a, 0x29f },
      { 0x164, 0x179, 0x190, 0x1a8, 0x1c1, 0x1dc, 0x1f8, 0x216, 0x236, 0x258, 0x27b, 0x2a1 },
      { 0x165, 0x17a, 0x191, 0x1a9, 0x1c2, 0x1dd, 0x1f9, 0x217, 0x237, 0x259, 0x27c, 0x2a2 },
      { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1de, 0x1fa, 0x218, 0x238, 0x25a, 0x27e, 0x2a3 },
      { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1df, 0x1fb, 0x219, 0x239, 0x25b, 0x27f, 0x2a4 },
      { 0x167, 0x17c, 0x193, 0x1ab, 0x1c4, 0x1e0, 0x1fc, 0x21a, 0x23a, 0x25c, 0x280, 0x2a6 },
      { 0x168, 0x17d, 0x194, 0x1ac, 0x1c5, 0x1e0, 0x1fd, 0x21b, 0x23b, 0x25d, 0x281, 0x2a7 },
      { 0x168, 0x17d, 0x194, 0x1ad, 0x1c6, 0x1e1, 0x1fe, 0x21c, 0x23c, 0x25e, 0x282, 0x2a8 },
      { 0x169, 0x17e, 0x195, 0x1ad, 0x1c7, 0x1e2, 0x1ff, 0x21d, 0x23d, 0x260, 0x283, 0x2a9 },
      { 0x16a, 0x17f, 0x196, 0x1ae, 0x1c8, 0x1e3, 0x1ff, 0x21e, 0x23e, 0x261, 0x284, 0x2ab },
      { 0x16a, 0x17f, 0x197, 0x1af, 0x1c8, 0x1e4, 0x200, 0x21f, 0x23f, 0x262, 0x286, 0x2ac }
   };

// Slot numbers as a function of the voice and the operator.
// ( melodic only)

static int slotVoice[ NUM_VOICES ][ 2 ] =
   {
      { 0, 3 },    // voice 0
      { 1, 4 },    // 1
      { 2, 5 },    // 2
      { 6, 9 },    // 3
      { 7, 10 },   // 4
      { 8, 11 },   // 5
      { 12, 15 },  // 6
      { 13, 16 },  // 7
      { 14, 17 },  // 8
   };

static int VoiceLevel[ NumChipSlots ][ 2 ];
static int VoiceKsl[ NumChipSlots ][ 2 ];

// This table gives the offset of each slot within the chip.
// offset = fn( slot)

static char offsetSlot[ NumChipSlots ] =
   {
    0,  1,  2,  3,  4,  5,
    8,  9, 10, 11, 12, 13,
   16, 17, 18, 19, 20, 21
   };

static int VoiceReserved[ NUM_VOICES * 2 ] =
   {
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
   };

static VOICE     Voice[ NUM_VOICES * 2 ];
static VOICELIST Voice_Pool;

static CHANNEL   Channel[ NUM_CHANNELS ];

static int AL_LeftPort   = 0x388;
static int AL_RightPort  = 0x388;
static int AL_Stereo     = FALSE;
static int AL_SendStereo = FALSE;
static int AL_OPL3       = FALSE;

static int AL_MaxMidiChannel = 16;


/*---------------------------------------------------------------------
   Function: AL_SendOutputToPort

   Sends data to the Adlib using a specified port.
---------------------------------------------------------------------*/

static void AL_SendOutputToPort
   (
   int  port,
   int  reg,
   int  data
   )

   {
   int delay;

   outp( port, reg );

   for( delay = 6; delay > 0 ; delay-- )
//   for( delay = 2; delay > 0 ; delay-- )
      {
      inp( port );
      }

   outp( port + 1, data );

//   for( delay = 35; delay > 0 ; delay-- )
   for( delay = 27; delay > 0 ; delay-- )
//   for( delay = 2; delay > 0 ; delay-- )
      {
      inp( port );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_SendOutput

   Sends data to the Adlib.
---------------------------------------------------------------------*/

static void AL_SendOutput
   (
   int  voice,
   int  reg,
   int  data
   )

   {
   int port;

   if ( AL_SendStereo )
      {
      AL_SendOutputToPort( AL_LeftPort, reg, data );
      AL_SendOutputToPort( AL_RightPort, reg, data );
      }
   else
      {
      port = ( voice == 0 ) ? AL_RightPort : AL_LeftPort;
      AL_SendOutputToPort( port, reg, data );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_SetVoiceTimbre

   Programs the specified voice's timbre.
---------------------------------------------------------------------*/

static void AL_SetVoiceTimbre
   (
   int voice
   )

   {
   int    off;
   int    slot;
   int    port;
   int    voc;
   int    patch;
   int    channel;
   TIMBRE *timbre;

   channel = Voice[ voice ].channel;

   if ( channel == 9 )
      {
      patch = Voice[ voice ].key + 128;
      }
   else
      {
      patch = Channel[ channel ].Timbre;
      }

   if ( Voice[ voice ].timbre == patch )
      {
      return;
      }

   Voice[ voice ].timbre = patch;
   timbre = &ADLIB_TimbreBank[ patch ];

   port = Voice[ voice ].port;
   voc  = ( voice >= NUM_VOICES ) ? voice - NUM_VOICES : voice;
   slot = slotVoice[ voc ][ 0 ];
   off  = offsetSlot[ slot ];

   VoiceLevel[ slot ][ port ] = 63 - ( timbre->Level[ 0 ] & 0x3f );
   VoiceKsl[ slot ][ port ]   = timbre->Level[ 0 ] & 0xc0;

   AL_SendOutput( port, 0xA0 + voc, 0 );
   AL_SendOutput( port, 0xB0 + voc, 0 );

   // Let voice clear the release
   AL_SendOutput( port, 0x80 + off, 0xff );

   AL_SendOutput( port, 0x60 + off, timbre->Env1[ 0 ] );
   AL_SendOutput( port, 0x80 + off, timbre->Env2[ 0 ] );
   AL_SendOutput( port, 0x20 + off, timbre->SAVEK[ 0 ] );
   AL_SendOutput( port, 0xE0 + off, timbre->Wave[ 0 ] );

   AL_SendOutput( port, 0x40 + off, timbre->Level[ 0 ] );
   slot = slotVoice[ voc ][ 1 ];

   if ( AL_SendStereo )
      {
      AL_SendOutputToPort( AL_LeftPort, 0xC0 + voice,
         ( timbre->Feedback & 0x0f ) | 0x20 );
      AL_SendOutputToPort( AL_RightPort, 0xC0 + voice,
         ( timbre->Feedback & 0x0f ) | 0x10 );
      }
   else
      {
      if ( AL_OPL3 )
         {
         AL_SendOutput( port, 0xC0 + voc, ( timbre->Feedback & 0x0f ) |
            0x30 );
         }
      else
         {
         AL_SendOutputToPort( ADLIB_PORT, 0xC0 + voice, timbre->Feedback );
         }
      }

   off = offsetSlot[ slot ];

   VoiceLevel[ slot ][ port ] = 63 - ( timbre->Level[ 1 ] & 0x3f );
   VoiceKsl[ slot ][ port ]   = timbre->Level[ 1 ] & 0xc0;
   AL_SendOutput( port, 0x40 + off, 63 );

   // Let voice clear the release
   AL_SendOutput( port, 0x80 + off, 0xff );

   AL_SendOutput( port, 0x60 + off, timbre->Env1[ 1 ] );
   AL_SendOutput( port, 0x80 + off, timbre->Env2[ 1 ] );
   AL_SendOutput( port, 0x20 + off, timbre->SAVEK[ 1 ] );
   AL_SendOutput( port, 0xE0 + off, timbre->Wave[ 1 ] );
   }


/*---------------------------------------------------------------------
   Function: AL_SetVoiceVolume

   Sets the volume of the specified voice.
---------------------------------------------------------------------*/

static void AL_SetVoiceVolume
   (
   int voice
   )

   {
   int channel;
   int velocity;
   int slot;
   int port;
   int voc;
   unsigned long t1;
   unsigned long t2;
   unsigned long volume;
   TIMBRE *timbre;

   channel = Voice[ voice ].channel;

   timbre = &ADLIB_TimbreBank[ Voice[ voice ].timbre ];

   velocity = Voice[ voice ].velocity + timbre->Velocity;
   velocity = min( velocity, MAX_VELOCITY );

   voc  = ( voice >= NUM_VOICES ) ? voice - NUM_VOICES : voice;

   slot = slotVoice[ voc ][ 1 ];
   port = Voice[ voice ].port;

   // amplitude
   t1  = ( unsigned )VoiceLevel[ slot ][ port ];
   t1 *= ( velocity + 0x80 );
   t1  = ( Channel[ channel ].Volume * t1 ) >> 15;

   if ( !AL_SendStereo )
      {
      volume  = t1 ^ 63;
      volume |= ( unsigned )VoiceKsl[ slot ][ port ];

      AL_SendOutput( port, 0x40 + offsetSlot[ slot ], volume );

      // Check if this timbre is Additive
      if ( timbre->Feedback & 0x01 )
         {
         slot = slotVoice[ voc ][ 0 ];

         // amplitude
         t2  = ( unsigned )VoiceLevel[ slot ][ port ];
         t2 *= ( velocity + 0x80 );
         t2  = ( Channel[ channel ].Volume * t1 ) >> 15;

         volume  = t2 ^ 63;
         volume |= ( unsigned )VoiceKsl[ slot ][ port ];

         AL_SendOutput( port, 0x40 + offsetSlot[ slot ], volume );
         }
      }
   else
      {
      // Set left channel volume
      volume = t1;
      if ( Channel[ channel ].Pan < 64 )
         {
         volume *= Channel[ channel ].Pan;
         volume >>= 6;
         }

      volume ^= 63;
      volume |= ( unsigned )VoiceKsl[ slot ][ port ];

      AL_SendOutputToPort( AL_LeftPort, 0x40 + offsetSlot[ slot ], volume );

      // Set right channel volume
      volume = t1;
      if ( Channel[ channel ].Pan > 64 )
         {
         volume *= 127 - Channel[ channel ].Pan;
         volume >>= 6;
         }

      volume ^= 63;
      volume |= ( unsigned )VoiceKsl[ slot ][ port ];

      AL_SendOutputToPort( AL_RightPort, 0x40 + offsetSlot[ slot ], volume );

      // Check if this timbre is Additive
      if ( timbre->Feedback & 0x01 )
         {
         // amplitude
         t2  = ( unsigned )VoiceLevel[ slot ][ port ];
         t2 *= ( velocity + 0x80 );
         t2  = ( Channel[ channel ].Volume * t1 ) >> 15;

         slot = slotVoice[ voc ][ 0 ];

         // Set left channel volume
         volume = t2;
         if ( Channel[ channel ].Pan < 64 )
            {
            volume *= Channel[ channel ].Pan;
            volume >>= 6;
            }

         volume ^= 63;
         volume |= ( unsigned )VoiceKsl[ slot ][ port ];

         AL_SendOutputToPort( AL_LeftPort, 0x40 + offsetSlot[ slot ], volume );

         // Set right channel volume
         volume = t2;
         if ( Channel[ channel ].Pan > 64 )
            {
            volume *= 127 - Channel[ channel ].Pan;
            volume >>= 6;
            }

         volume ^= 63;
         volume |= ( unsigned )VoiceKsl[ slot ][ port ];

         AL_SendOutputToPort( AL_RightPort, 0x40 + offsetSlot[ slot ], volume );
         }
      }
   }


/*---------------------------------------------------------------------
   Linked list management routines.
---------------------------------------------------------------------*/

enum LL_Errors
   {
   LL_Warning = -2,
   LL_Error   = -1,
   LL_Ok      = 0
   };

typedef struct list
   {
   void *start;
   void *end;
   } list;

#define OFFSET( structure, offset ) \
   ( *( ( char ** )&( structure )[ offset ] ) )

#define LL_AddToHead( type, listhead, node )         \
    LL_AddNode( ( char * )( node ),                  \
                ( char ** )&( ( listhead )->start ), \
                ( char ** )&( ( listhead )->end ),   \
                ( int )&( ( type * ) 0 )->next,      \
                ( int )&( ( type * ) 0 )->prev )

#define LL_AddToTail( type, listhead, node )         \
    LL_AddNode( ( char * )( node ),                  \
                ( char ** )&( ( listhead )->end ),   \
                ( char ** )&( ( listhead )->start ), \
                ( int )&( ( type * ) 0 )->prev,      \
                ( int )&( ( type * ) 0 )->next )

#define LL_Remove( type, listhead, node )               \
    LL_RemoveNode( ( char * )( node ),                  \
                   ( char ** )&( ( listhead )->start ), \
                   ( char ** )&( ( listhead )->end ),   \
                   ( int )&( ( type * ) 0 )->next,      \
                   ( int )&( ( type * ) 0 )->prev )

#define LL_NextNode( node )     ( ( node )->next )
#define LL_PreviousNode( node ) ( ( node )->prev )

static void LL_AddNode
   (
   char *item,
   char **head,
   char **tail,
   int next,
   int prev
   )

   {
   OFFSET( item, prev ) = NULL;
   OFFSET( item, next ) = *head;

   if ( *head )
      {
      OFFSET( *head, prev ) = item;
      }
   else
      {
      *tail = item;
      }

   *head = item;
   }

static void LL_RemoveNode
   (
   char *item,
   char **head,
   char **tail,
   int next,
   int prev
   )

   {
   if ( OFFSET( item, prev ) == NULL )
      {
      *head = OFFSET( item, next );
      }
   else
      {
      OFFSET( OFFSET( item, prev ), next ) = OFFSET( item, next );
      }

   if ( OFFSET( item, next ) == NULL )
      {
      *tail = OFFSET( item, prev );
      }
   else
      {
      OFFSET( OFFSET( item, next ), prev ) = OFFSET( item, prev );
      }

   OFFSET( item, next ) = NULL;
   OFFSET( item, prev ) = NULL;
   }


/*---------------------------------------------------------------------
   Function: AL_AllocVoice

   Retrieves a free voice from the voice pool.
---------------------------------------------------------------------*/

static int AL_AllocVoice
   (
   void
   )

   {
   int voice;

   if ( Voice_Pool.start )
      {
      voice = Voice_Pool.start->num;
      LL_Remove( VOICE, &Voice_Pool, &Voice[ voice ] );
      return( voice );
      }

   return( AL_VoiceNotFound );
   }


/*---------------------------------------------------------------------
   Function: AL_GetVoice

   Determines which voice is associated with a specified note and
   MIDI channel.
---------------------------------------------------------------------*/

static int AL_GetVoice
   (
   int channel,
   int key
   )

   {
   VOICE *voice;

   voice = Channel[ channel ].Voices.start;

   while( voice != NULL )
      {
      if ( voice->key == key )
         {
         return( voice->num );
         }
      voice = voice->next;
      }

   return( AL_VoiceNotFound );
   }


/*---------------------------------------------------------------------
   Function: AL_SetVoicePitch

   Programs the pitch of the specified voice.
---------------------------------------------------------------------*/

static void AL_SetVoicePitch
   (
   int voice
   )

   {
   int note;
   int channel;
   int patch;
   int detune;
   int ScaleNote;
   int Octave;
   int pitch;
   int port;
   int voc;

   port = Voice[ voice ].port;
   voc  = ( voice >= NUM_VOICES ) ? voice - NUM_VOICES : voice;

   channel = Voice[ voice ].channel;

   if ( channel == 9 )
      {
      patch = Voice[ voice ].key + 128;
      note  = ADLIB_TimbreBank[ patch ].Transpose;
      }
   else
      {
      patch = Channel[ channel ].Timbre;
      note  = Voice[ voice ].key + ADLIB_TimbreBank[ patch ].Transpose;
      }

   note += Channel[ channel ].KeyOffset - 12;

   if ( note > MAX_NOTE )
      {
      note = MAX_NOTE;
      }
   if ( note < 0 )
      {
      note = 0;
      }

   detune = Channel[ channel ].KeyDetune;

   ScaleNote = NoteMod12[ note ];
   Octave    = NoteDiv12[ note ];

   pitch = OctavePitch[ Octave ] | NotePitch[ detune ][ ScaleNote ];

   Voice[ voice ].pitchleft = pitch;

   pitch |= Voice[ voice ].status;

   if ( !AL_SendStereo )
      {
      AL_SendOutput( port, 0xA0 + voc, pitch );
      AL_SendOutput( port, 0xB0 + voc, pitch >> 8 );
      }
   else
      {
      AL_SendOutputToPort( AL_LeftPort, 0xA0 + voice, pitch );
      AL_SendOutputToPort( AL_LeftPort, 0xB0 + voice, pitch >> 8 );

      if ( channel != 9 )
         {
         detune += STEREO_DETUNE;
         }

      if ( detune > FINETUNE_MAX )
         {
         detune -= FINETUNE_RANGE;
         if ( note < MAX_NOTE )
            {
            note++;
            ScaleNote = NoteMod12[ note ];
            Octave    = NoteDiv12[ note ];
            }
         }

      pitch = OctavePitch[ Octave ] | NotePitch[ detune ][ ScaleNote ];

      Voice[ voice ].pitchright = pitch;

      pitch |= Voice[ voice ].status;

      AL_SendOutputToPort( AL_RightPort, 0xA0 + voice, pitch );
      AL_SendOutputToPort( AL_RightPort, 0xB0 + voice, pitch >> 8 );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_SetChannelVolume

   Sets the volume of the specified MIDI channel.
---------------------------------------------------------------------*/

static void AL_SetChannelVolume
   (
   int channel,
   int volume
   )

   {
   VOICE *voice;

   volume = max( 0, volume );
   volume = min( volume, AL_MaxVolume );
   Channel[ channel ].Volume = volume;

   voice = Channel[ channel ].Voices.start;
   while( voice != NULL )
      {
      AL_SetVoiceVolume( voice->num );
      voice = voice->next;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_SetChannelPan

   Sets the pan position of the specified MIDI channel.
---------------------------------------------------------------------*/

static void AL_SetChannelPan
   (
   int channel,
   int pan
   )

   {
   // Don't pan drum sounds
   if ( channel != 9 )
      {
      Channel[ channel ].Pan = pan;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_SetChannelDetune

   Sets the stereo voice detune of the specified MIDI channel.
---------------------------------------------------------------------*/

static void AL_SetChannelDetune
   (
   int channel,
   int detune
   )

   {
   Channel[ channel ].Detune = detune;
   }


/*---------------------------------------------------------------------
   Function: AL_ResetVoices

   Sets all voice info to the default state.
---------------------------------------------------------------------*/

static void AL_ResetVoices
   (
   void
   )

   {
   int index;
   int numvoices;

   Voice_Pool.start = NULL;
   Voice_Pool.end = NULL;

   numvoices = NUM_VOICES;
   if ( ( AL_OPL3 ) && ( !AL_Stereo ) )
      {
      numvoices = NUM_VOICES * 2;
      }
   for( index = 0; index < numvoices; index++ )
      {
      if ( VoiceReserved[ index ] == FALSE )
         {
         Voice[ index ].num = index;
         Voice[ index ].key = 0;
         Voice[ index ].velocity = 0;
         Voice[ index ].channel = -1;
         Voice[ index ].timbre = -1;
         Voice[ index ].port = ( index < NUM_VOICES ) ? 0 : 1;
         Voice[ index ].status = NOTE_OFF;
         LL_AddToTail( VOICE, &Voice_Pool, &Voice[ index ] );
         }
      }

   for( index = 0; index < NUM_CHANNELS; index++ )
      {
      Channel[ index ].Voices.start    = NULL;
      Channel[ index ].Voices.end      = NULL;
      Channel[ index ].Timbre          = 0;
      Channel[ index ].Pitchbend       = 0;
      Channel[ index ].KeyOffset       = 0;
      Channel[ index ].KeyDetune       = 0;
      Channel[ index ].Volume          = AL_DefaultChannelVolume;
      Channel[ index ].Pan             = 64;
      Channel[ index ].RPN             = 0;

      Channel[ index ].PitchBendRange     = AL_DefaultPitchBendRange;
      Channel[ index ].PitchBendSemiTones = AL_DefaultPitchBendRange / 100;
      Channel[ index ].PitchBendHundreds  = AL_DefaultPitchBendRange % 100;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_CalcPitchInfo

   Calculates the pitch table.
---------------------------------------------------------------------*/

static void AL_CalcPitchInfo
   (
   void
   )

   {
   int    note;

   for( note = 0; note <= MAX_NOTE; note++ )
      {
      NoteMod12[ note ] = note % 12;
      NoteDiv12[ note ] = note / 12;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_FlushCard

   Sets all voices to a known (quiet) state.
---------------------------------------------------------------------*/

static void AL_FlushCard
   (
   int port
   )

   {
   int i;
   unsigned slot1;
   unsigned slot2;

   for( i = 0 ; i < NUM_VOICES; i++ )
      {
      if ( VoiceReserved[ i ] == FALSE )
         {
         slot1 = offsetSlot[ slotVoice[ i ][ 0 ] ];
         slot2 = offsetSlot[ slotVoice[ i ][ 1 ] ];

         AL_SendOutputToPort( port, 0xA0 + i, 0 );
         AL_SendOutputToPort( port, 0xB0 + i, 0 );

         AL_SendOutputToPort( port, 0xE0 + slot1, 0 );
         AL_SendOutputToPort( port, 0xE0 + slot2, 0 );

         // Set the envelope to be fast and quiet
         AL_SendOutputToPort( port, 0x60 + slot1, 0xff );
         AL_SendOutputToPort( port, 0x60 + slot2, 0xff );
         AL_SendOutputToPort( port, 0x80 + slot1, 0xff );
         AL_SendOutputToPort( port, 0x80 + slot2, 0xff );

         // Maximum attenuation
         AL_SendOutputToPort( port, 0x40 + slot1, 0xff );
         AL_SendOutputToPort( port, 0x40 + slot2, 0xff );
         }
      }
   }


/*---------------------------------------------------------------------
   Function: AL_StereoOn

   Sets the card send info in stereo.
---------------------------------------------------------------------*/

static void AL_StereoOn
   (
   void
   )

   {
   if ( ( AL_Stereo ) && ( !AL_SendStereo ) )
      {
      AL_SendStereo = TRUE;
      if ( AL_OPL3 )
         {
         // Set card to OPL3 operation
         AL_SendOutputToPort( AL_RightPort, 0x5, 1 );
         }
      }
   else if ( AL_OPL3 )
      {
      // Set card to OPL3 operation
      AL_SendOutputToPort( AL_RightPort, 0x5, 1 );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_StereoOff

   Sets the card send info in mono.
---------------------------------------------------------------------*/

static void AL_StereoOff
   (
   void
   )

   {
   if ( ( AL_Stereo ) && ( AL_SendStereo ) )
      {
      AL_SendStereo = FALSE;
      if ( AL_OPL3 )
         {
         // Set card back to OPL2 operation
         AL_SendOutputToPort( AL_RightPort, 0x5, 0 );
         }
      }
   else if ( AL_OPL3 )
      {
      // Set card back to OPL2 operation
      AL_SendOutputToPort( AL_RightPort, 0x5, 0 );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_Reset

   Sets the card to a known (quiet) state.
---------------------------------------------------------------------*/

static void AL_Reset
   (
   void
   )

   {
   AL_SendOutputToPort( ADLIB_PORT, 1, 0x20 );
   AL_SendOutputToPort( ADLIB_PORT, 0x08, 0 );

   // Set the values: AM Depth, VIB depth & Rhythm
   AL_SendOutputToPort( ADLIB_PORT, 0xBD, 0 );

   AL_StereoOn();

   if ( ( AL_SendStereo ) || ( AL_OPL3 ) )
      {
      AL_FlushCard( AL_LeftPort );
      AL_FlushCard( AL_RightPort );
      }
   else
      {
      AL_FlushCard( ADLIB_PORT );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_NoteOff

   Turns off a note on the specified MIDI channel.
---------------------------------------------------------------------*/

void AL_NoteOff
   (
   int channel,
   int key,
   int velocity
   )

   {
   int voice;
   int port;
   int voc;

   // We only play channels 1 through 10
   if ( channel > AL_MaxMidiChannel )
      {
      return;
      }

   voice = AL_GetVoice( channel, key );

   if ( voice == AL_VoiceNotFound )
      {
      return;
      }

   Voice[ voice ].status = NOTE_OFF;

   port = Voice[ voice ].port;
   voc  = ( voice >= NUM_VOICES ) ? voice - NUM_VOICES : voice;

   if ( AL_SendStereo )
      {
      AL_SendOutputToPort( AL_LeftPort, 0xB0 + voice,
         hibyte( Voice[ voice ].pitchleft ) );
      AL_SendOutputToPort( AL_RightPort, 0xB0 + voice,
         hibyte( Voice[ voice ].pitchright ) );
      }
   else
      {
      AL_SendOutput( port, 0xB0 + voc, hibyte( Voice[ voice ].pitchleft ) );
      }

   LL_Remove( VOICE, &Channel[ channel ].Voices, &Voice[ voice ] );
   LL_AddToTail( VOICE, &Voice_Pool, &Voice[ voice ] );
   }


/*---------------------------------------------------------------------
   Function: AL_NoteOn

   Plays a note on the specified MIDI channel.
---------------------------------------------------------------------*/

void AL_NoteOn
   (
   int channel,
   int key,
   int velocity
   )

   {
   int voice;

   // We only play channels 1 through 10
   if ( channel > AL_MaxMidiChannel )
      {
      return;
      }

   if ( velocity == 0 )
      {
      AL_NoteOff( channel, key, velocity );
      return;
      }

   voice = AL_AllocVoice();

   if ( voice == AL_VoiceNotFound )
      {
      if ( Channel[ 9 ].Voices.start )
         {
         AL_NoteOff( 9, Channel[ 9 ].Voices.start->key, 0 );
         voice = AL_AllocVoice();
         }
      if ( voice == AL_VoiceNotFound )
         {
         return;
         }
      }

   Voice[ voice ].key      = key;
   Voice[ voice ].channel  = channel;
   Voice[ voice ].velocity = velocity;
   Voice[ voice ].status   = NOTE_ON;

   LL_AddToTail( VOICE, &Channel[ channel ].Voices, &Voice[ voice ] );

   AL_SetVoiceTimbre( voice );
   AL_SetVoiceVolume( voice );
   AL_SetVoicePitch( voice );
   }


/*---------------------------------------------------------------------
   Function: AL_AllNotesOff

   Turns off all currently playing voices.
---------------------------------------------------------------------*/

static void AL_AllNotesOff
   (
   int channel
   )

   {
   while( Channel[ channel ].Voices.start != NULL )
      {
      AL_NoteOff( channel, Channel[ channel ].Voices.start->key, 0 );
      }
   }


/*---------------------------------------------------------------------
   Function: AL_ControlChange

   Sets the value of a controller on the specified MIDI channel.
---------------------------------------------------------------------*/

void AL_ControlChange
   (
   int channel,
   int type,
   int data
   )

   {
   // We only play channels 1 through 10
   if ( channel > AL_MaxMidiChannel )
      {
      return;
      }

   switch( type )
      {
      case MIDI_VOLUME :
         AL_SetChannelVolume( channel, data );
         break;

      case MIDI_PAN :
         AL_SetChannelPan( channel, data );
         break;

      case MIDI_DETUNE :
         AL_SetChannelDetune( channel, data );
         break;

      case MIDI_ALL_NOTES_OFF :
         AL_AllNotesOff( channel );
         break;

      case MIDI_RESET_ALL_CONTROLLERS :
         AL_ResetVoices();
         AL_SetChannelVolume( channel, AL_DefaultChannelVolume );
         AL_SetChannelPan( channel, 64 );
         AL_SetChannelDetune( channel, 0 );
         break;

      case MIDI_RPN_MSB :
         Channel[ channel ].RPN &= 0x00FF;
         Channel[ channel ].RPN |= ( data & 0xFF ) << 8;
         break;

      case MIDI_RPN_LSB :
         Channel[ channel ].RPN &= 0xFF00;
         Channel[ channel ].RPN |= data & 0xFF;
         break;

      case MIDI_DATAENTRY_MSB :
         if ( Channel[ channel ].RPN == MIDI_PITCHBEND_RPN )
            {
            Channel[ channel ].PitchBendSemiTones = data;
            Channel[ channel ].PitchBendRange     =
               Channel[ channel ].PitchBendSemiTones * 100 +
               Channel[ channel ].PitchBendHundreds;
            }
         break;

      case MIDI_DATAENTRY_LSB :
         if ( Channel[ channel ].RPN == MIDI_PITCHBEND_RPN )
            {
            Channel[ channel ].PitchBendHundreds = data;
            Channel[ channel ].PitchBendRange    =
               Channel[ channel ].PitchBendSemiTones * 100 +
               Channel[ channel ].PitchBendHundreds;
            }
         break;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_ProgramChange

   Selects the instrument to use on the specified MIDI channel.
---------------------------------------------------------------------*/

void AL_ProgramChange
   (
   int channel,
   int patch
   )

   {
   // We only play channels 1 through 10
   if ( channel > AL_MaxMidiChannel )
      {
      return;
      }

   Channel[ channel ].Timbre  = patch;
   }


/*---------------------------------------------------------------------
   Function: AL_SetPitchBend

   Sets the pitch bend amount on the specified MIDI channel.
---------------------------------------------------------------------*/

void AL_SetPitchBend
   (
   int channel,
   int lsb,
   int msb
   )

   {
   int            pitchbend;
   unsigned long  TotalBend;
   VOICE         *voice;

   // We only play channels 1 through 10
   if ( channel > AL_MaxMidiChannel )
      {
      return;
      }

   pitchbend = lsb + ( msb << 8 );
   Channel[ channel ].Pitchbend = pitchbend;

   TotalBend  = pitchbend * Channel[ channel ].PitchBendRange;
   TotalBend /= ( PITCHBEND_CENTER / FINETUNE_RANGE );

   Channel[ channel ].KeyOffset  = ( int )( TotalBend / FINETUNE_RANGE );
   Channel[ channel ].KeyOffset -= Channel[ channel ].PitchBendSemiTones;

   Channel[ channel ].KeyDetune = ( unsigned )( TotalBend % FINETUNE_RANGE );

   voice = Channel[ channel ].Voices.start;
   while( voice != NULL )
      {
      AL_SetVoicePitch( voice->num );
      voice = voice->next;
      }
   }


/*---------------------------------------------------------------------
   Function: AL_DetectFM

   Determines if an Adlib compatible card is installed in the machine.
---------------------------------------------------------------------*/

int AL_DetectFM
   (
   void
   )

   {
   int status1;
   int status2;
   int i;

   AL_SendOutputToPort( ADLIB_PORT, 4, 0x60 );   // Reset T1 & T2
   AL_SendOutputToPort( ADLIB_PORT, 4, 0x80 );   // Reset IRQ

   status1 = inp( ADLIB_PORT );

   AL_SendOutputToPort( ADLIB_PORT, 2, 0xff );   // Set timer 1
   AL_SendOutputToPort( ADLIB_PORT, 4, 0x21 );   // Start timer 1

   for( i = 100; i > 0; i-- )
      {
      inp( ADLIB_PORT );
      }

   status2 = inp( ADLIB_PORT );

   AL_SendOutputToPort( ADLIB_PORT, 4, 0x60 );
   AL_SendOutputToPort( ADLIB_PORT, 4, 0x80 );

   return( ( ( status1 & 0xe0 ) == 0x00 ) && ( ( status2 & 0xe0 ) == 0xc0 ) );
   }


/*---------------------------------------------------------------------
   Function: AL_Shutdown

   Ends use of the sound card and resets it to a quiet state.
---------------------------------------------------------------------*/

void AL_Shutdown
   (
   void
   )

   {
   AL_StereoOff();

   AL_OPL3 = FALSE;
   AL_ResetVoices();
   AL_Reset();
   }


/*---------------------------------------------------------------------
   Function: AL_Init

   Begins use of the sound card.
---------------------------------------------------------------------*/

int AL_Init
   (
   int soundcard
   )

   {
   BLASTER_CONFIG Blaster;
   int status;

   AL_Stereo = FALSE;
   AL_OPL3   = FALSE;
   AL_LeftPort = 0x388;
   AL_RightPort = 0x388;

   switch( soundcard )
      {
      case ProAudioSpectrum :
      case SoundMan16 :
         AL_OPL3 = TRUE;
         AL_LeftPort = 0x388;
         AL_RightPort = 0x38A;
         break;

      case SoundBlaster :
         status = BLASTER_GetCardSettings( &Blaster );
         if ( status != BLASTER_Ok )
            {
            status = BLASTER_GetEnv( &Blaster );
            if ( status != BLASTER_Ok )
               {
               break;
               }
            }

         switch( Blaster.Type )
            {
            case SBPro2 :
            case SB16 :
               AL_OPL3 = TRUE;
               AL_LeftPort  = Blaster.Address;
               AL_RightPort = Blaster.Address + 2;
               break;
            }
         break;
      }

   AL_CalcPitchInfo();
   AL_Reset();
   AL_ResetVoices();

   return( AL_Ok );
   }


/*---------------------------------------------------------------------
   Function: AL_RegisterTimbreBank

   Copies user supplied timbres over the default timbre bank.
---------------------------------------------------------------------*/

void AL_RegisterTimbreBank
   (
   unsigned char *timbres
   )

   {
   int i;

   for( i = 0; i < 256; i++ )
      {
      ADLIB_TimbreBank[ i ].SAVEK[ 0 ] = *( timbres++ );
      ADLIB_TimbreBank[ i ].SAVEK[ 1 ] = *( timbres++ );
      ADLIB_TimbreBank[ i ].Level[ 0 ] = *( timbres++ );
      ADLIB_TimbreBank[ i ].Level[ 1 ] = *( timbres++ );
      ADLIB_TimbreBank[ i ].Env1[ 0 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Env1[ 1 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Env2[ 0 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Env2[ 1 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Wave[ 0 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Wave[ 1 ]  = *( timbres++ );
      ADLIB_TimbreBank[ i ].Feedback   = *( timbres++ );
      ADLIB_TimbreBank[ i ].Transpose  = *( signed char * )( timbres++ );
      ADLIB_TimbreBank[ i ].Velocity   = *( signed char * )( timbres++ );
      }
   }
