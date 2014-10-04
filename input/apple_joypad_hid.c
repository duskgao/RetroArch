/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "apple_input.h"
#include "input_common.h"
#include "../general.h"

#include "wiimote.c"
#include "apple_joypad_ps3.c"
#include "apple_joypad_ps4.c"
#include "apple_joypad_wii.c"

typedef struct
{
   bool used;
   struct apple_pad_interface* iface;
   void* data;
   
   bool is_gcapi;
} joypad_slot_t;

static joypad_slot_t slots[MAX_PLAYERS];

static int32_t find_empty_slot(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!slots[i].used)
      {
         memset(&slots[i], 0, sizeof(slots[0]));
         return i;
      }
   }
   return -1;
}

int32_t apple_joypad_connect(const char* name, void *data)
{
   struct apple_pad_connection* connection = 
      (struct apple_pad_connection*)data;
   int32_t slot = find_empty_slot();

   if (slot >= 0 && slot < MAX_PLAYERS)
   {
      unsigned i;
      joypad_slot_t* s = (joypad_slot_t*)&slots[slot];
      s->used = true;

      static const struct
      {
         const char* name;
         struct apple_pad_interface* iface;
      } pad_map[] = 
      {
         { "Nintendo RVL-CNT-01",         &apple_pad_wii },
         /* { "Nintendo RVL-CNT-01-UC",   &apple_pad_wii }, */ /* WiiU */
         /* { "Wireless Controller",         &apple_pad_ps4 }, */ /* DualShock4 */
         { "PLAYSTATION(R)3 Controller",  &apple_pad_ps3 },
         { 0, 0}
      };

      for (i = 0; name && pad_map[i].name; i++)
         if (strstr(name, pad_map[i].name))
         {
            s->iface = pad_map[i].iface;
            s->data = s->iface->connect(connection, slot);
         }
   }

   return slot;
}

int32_t apple_joypad_connect_gcapi(void)
{
   int32_t slot = find_empty_slot();

   if (slot >= 0 && slot < MAX_PLAYERS)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[slot];

      s->used = true;
      s->is_gcapi = true;
   }

   return slot;
}

void apple_joypad_disconnect(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t* s = (joypad_slot_t*)&slots[slot];

      if (s->iface && s->data && s->iface->disconnect)
         s->iface->disconnect(s->data);

      memset(s, 0, sizeof(joypad_slot_t));
   }
}

void apple_joypad_packet(uint32_t slot,
      uint8_t* data, uint32_t length)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
   {
      joypad_slot_t *s = (joypad_slot_t*)&slots[slot];

      if (s->iface && s->data && s->iface->packet_handler)
         s->iface->packet_handler(s->data, data, length);
   }
}

bool apple_joypad_has_interface(uint32_t slot)
{
   if (slot < MAX_PLAYERS && slots[slot].used)
      return slots[slot].iface ? true : false;

   return false;
}

static bool apple_joypad_init(void)
{
   return hid_init_manager();
}

static bool apple_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS;
}

static void apple_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i ++)
   {
      if (slots[i].used && slots[i].iface && slots[i].iface->set_rumble)
      {
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_STRONG, 0);
         slots[i].iface->set_rumble(slots[i].data, RETRO_RUMBLE_WEAK, 0);
      }
   }
    
   hid_exit();
}

static bool apple_joypad_button(unsigned port, uint16_t joykey)
{
   apple_input_data_t *apple = (apple_input_data_t*)driver.input_data;
   if (!apple || joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
      return false;
   // Check the button
   return (port < MAX_PLAYERS && joykey < 32) ? 
      (apple->buttons[port] & (1 << joykey)) != 0 : false;
}

static int16_t apple_joypad_axis(unsigned port, uint32_t joyaxis)
{
   apple_input_data_t *apple = (apple_input_data_t*)driver.input_data;
   int16_t val = 0;

   if (!apple || joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val = apple->axes[port][AXIS_NEG_GET(joyaxis)];
      val = (val < 0) ? val : 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val = apple->axes[port][AXIS_POS_GET(joyaxis)];
      val = (val > 0) ? val : 0;
   }

   return val;
}

static void apple_joypad_poll(void)
{
}

static bool apple_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (pad < MAX_PLAYERS && slots[pad].used && slots[pad].iface
       && slots[pad].iface->set_rumble)
   {
      slots[pad].iface->set_rumble(slots[pad].data, effect, strength);
      return true;
   }

   return false;
}

static const char *apple_joypad_name(unsigned joypad)
{
   (void)joypad;
   return NULL;
}

rarch_joypad_driver_t apple_hid_joypad = {
   apple_joypad_init,
   apple_joypad_query_pad,
   apple_joypad_destroy,
   apple_joypad_button,
   apple_joypad_axis,
   apple_joypad_poll,
   apple_joypad_rumble,
   apple_joypad_name,
   "apple_hid"
};
