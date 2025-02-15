// license:BSD-3-Clause
// copyright-holders:Chris Moore
/***************************************************************************

GAME PLAN driver

driver by Chris Moore

Killer Comet memory map

MAIN CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
00000-xxxxxxxxxx R/W xxxxxxxx RAM       can be either 256 bytes (2x2101) or 1kB (2x2114)
00001-----------              n.c.
00010-----------              n.c.
00011-----------              n.c.
00100-------xxxx R/W xxxxxxxx VIA 1     6522 for video interface
00101-------xxxx R/W xxxxxxxx VIA 2     6522 for I/O interface
00110-------xxxx R/W xxxxxxxx VIA 3     6522 for interface with audio CPU
00111-----------              n.c.
01--------------              n.c.
10--------------              n.c.
11000xxxxxxxxxxx R   xxxxxxxx ROM E2    program ROM
11001xxxxxxxxxxx R   xxxxxxxx ROM F2    program ROM
11010xxxxxxxxxxx R   xxxxxxxx ROM G2    program ROM
11011xxxxxxxxxxx R   xxxxxxxx ROM J2    program ROM
11100xxxxxxxxxxx R   xxxxxxxx ROM J1    program ROM
11101xxxxxxxxxxx R   xxxxxxxx ROM G1    program ROM
11110xxxxxxxxxxx R   xxxxxxxx ROM F1    program ROM
11111xxxxxxxxxxx R   xxxxxxxx ROM E1    program ROM


SOUND CPU:

Address          Dir Data     Name      Description
---------------- --- -------- --------- -----------------------
000-0----xxxxxxx R/W xxxxxxxx VIA 5     6532 internal RAM
000-1------xxxxx R/W xxxxxxxx VIA 5     6532 for interface with main CPU
001-------------              n.c.
010-------------              n.c.
011-------------              n.c.
100-------------              n.c.
101-----------xx R/W xxxxxxxx PSG 1     AY-3-8910
110-------------              n.c.
111--xxxxxxxxxxx R   xxxxxxxx ROM E1


Notes:
- There are two dip switch banks connected to the 8910 ports. They are only
  used for testing.

- Megatack's test mode reports the same fire buttons as Killer Comet, but this
  is wrong: there is only one fire button, not three.

- Megatack's actual name which displays proudly on the cover and everywhere in
  the manual as "MEGATTACK"

- Checked and verified DIPs from manuals and service mode for:
  Challenger
  Kaos
  Killer Comet
  Megattack
  Pot Of Gold (Leprechaun)


TODO:
- The board has, instead of a watchdog, a timed reset that has to be disabled
  on startup. The disable line is tied to CA2 of VIA2, but I don't see writes
  to that pin in the log. Missing support in machine/6522via.cpp?
- Investigate and document the 8910 dip switches
- Fix the input ports of Kaos

****************************************************************************/

#include "emu.h"
#include "gameplan.h"

#include "cpu/m6502/m6502.h"
#include "sound/ay8910.h"
#include "speaker.h"



/*************************************
 *
 *  VIA 2 - I/O
 *
 *************************************/

void gameplan_state::io_select_w(uint8_t data)
{
	switch (data)
	{
	case 0x01: m_current_port = 0; break;
	case 0x02: m_current_port = 1; break;
	case 0x04: m_current_port = 2; break;
	case 0x08: m_current_port = 3; break;
	case 0x80: m_current_port = 4; break;
	case 0x40: m_current_port = 5; break;
	}
}


uint8_t gameplan_state::io_port_r()
{
	static const char *const portnames[] = { "IN0", "IN1", "IN2", "IN3", "DSW0", "DSW1" };

	return ioport(portnames[m_current_port])->read();
}


WRITE_LINE_MEMBER(gameplan_state::coin_w)
{
	machine().bookkeeping().coin_counter_w(0, ~state & 1);
}



/*************************************
 *
 *  VIA 3 - audio
 *
 *************************************/

WRITE_LINE_MEMBER(gameplan_state::audio_reset_w)
{
	m_audiocpu->set_input_line(INPUT_LINE_RESET, state ? CLEAR_LINE : ASSERT_LINE);

	if (state == 0)
	{
		m_riot->reset();
		machine().scheduler().boost_interleave(attotime::zero, attotime::from_usec(10));
	}
}


void gameplan_state::audio_cmd_w(uint8_t data)
{
	m_riot->porta_in_set(data, 0x7f);
}


WRITE_LINE_MEMBER(gameplan_state::audio_trigger_w)
{
	m_riot->porta_in_set(state << 7, 0x80);
}



/*************************************
 *
 *  RIOT - audio
 *
 *************************************/

WRITE_LINE_MEMBER(gameplan_state::r6532_irq)
{
	m_audiocpu->set_input_line(0, state);
	if (state == ASSERT_LINE)
		machine().scheduler().boost_interleave(attotime::zero, attotime::from_usec(10));
}



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

void gameplan_state::gameplan_main_map(address_map &map)
{
	map(0x0000, 0x03ff).mirror(0x1c00).ram();
	map(0x2000, 0x200f).mirror(0x07f0).m(m_via_0, FUNC(via6522_device::map));    /* VIA 1 */
	map(0x2800, 0x280f).mirror(0x07f0).m(m_via_1, FUNC(via6522_device::map));    /* VIA 2 */
	map(0x3000, 0x300f).mirror(0x07f0).m(m_via_2, FUNC(via6522_device::map));    /* VIA 3 */
	map(0x8000, 0xffff).rom();
}



/*************************************
 *
 *  Audio CPU memory handlers
 *
 *************************************/

void gameplan_state::gameplan_audio_map(address_map &map)
{
	map(0x0000, 0x007f).mirror(0x1780).ram();  /* 6532 internal RAM */
	map(0x0800, 0x081f).mirror(0x17e0).rw(m_riot, FUNC(riot6532_device::read), FUNC(riot6532_device::write));
	map(0xa000, 0xa000).mirror(0x1ffc).w("aysnd", FUNC(ay8910_device::address_w));
	map(0xa001, 0xa001).mirror(0x1ffc).r("aysnd", FUNC(ay8910_device::data_r));
	map(0xa002, 0xa002).mirror(0x1ffc).w("aysnd", FUNC(ay8910_device::data_w));
	map(0xe000, 0xe7ff).mirror(0x1800).rom();
}


/* same as Gameplan, but larger ROM */
void gameplan_state::leprechn_audio_map(address_map &map)
{
	map(0x0000, 0x007f).mirror(0x1780).ram();  /* 6532 internal RAM */
	map(0x0800, 0x081f).mirror(0x17e0).rw(m_riot, FUNC(riot6532_device::read), FUNC(riot6532_device::write));
	map(0xa000, 0xa000).mirror(0x1ffc).w("aysnd", FUNC(ay8910_device::address_w));
	map(0xa001, 0xa001).mirror(0x1ffc).r("aysnd", FUNC(ay8910_device::data_r));
	map(0xa002, 0xa002).mirror(0x1ffc).w("aysnd", FUNC(ay8910_device::data_w));
	map(0xe000, 0xefff).mirror(0x1000).rom();
}



/*************************************
 *
 *  Input ports
 *
 *************************************/

static INPUT_PORTS_START( killcom )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")  /* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x08, "5" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPNAME( 0xc0, 0xc0, "Reaction" ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, "Slowest" )
	PORT_DIPSETTING(    0x80, "Slow" )
	PORT_DIPSETTING(    0x40, "Fast" )
	PORT_DIPSETTING(    0x00, "Fastest" )

	PORT_START("DSW1")  /* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW2:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW2:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END


static INPUT_PORTS_START( megatack )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")  /* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW1:3" )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW1:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW1:8" )

	PORT_START("DSW1")  /* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:1,2,3")
	PORT_DIPSETTING(    0x07, "20000" )
	PORT_DIPSETTING(    0x06, "30000" )
	PORT_DIPSETTING(    0x05, "40000" )
	PORT_DIPSETTING(    0x04, "50000" )
	PORT_DIPSETTING(    0x03, "60000" )
	PORT_DIPSETTING(    0x02, "70000" )
	PORT_DIPSETTING(    0x01, "80000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPNAME( 0x10, 0x10, "Monitor View" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "Direct" )
	PORT_DIPSETTING(    0x00, "Mirror" )
	PORT_DIPNAME( 0x20, 0x20, "Monitor Orientation" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Horizontal" )
	PORT_DIPSETTING(    0x00, "Vertical" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test A 0" ) PORT_DIPLOCATION("SW3:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test A 1" ) PORT_DIPLOCATION("SW3:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test A 2" ) PORT_DIPLOCATION("SW3:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test A 3" ) PORT_DIPLOCATION("SW3:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test A 4" ) PORT_DIPLOCATION("SW3:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test A 5" ) PORT_DIPLOCATION("SW3:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test A 6" ) PORT_DIPLOCATION("SW3:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Sound Test Enable" ) PORT_DIPLOCATION("SW3:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test B 0" ) PORT_DIPLOCATION("SW4:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test B 1" ) PORT_DIPLOCATION("SW4:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test B 2" ) PORT_DIPLOCATION("SW4:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test B 3" ) PORT_DIPLOCATION("SW4:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test B 4" ) PORT_DIPLOCATION("SW4:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test B 5" ) PORT_DIPLOCATION("SW4:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test B 6" ) PORT_DIPLOCATION("SW4:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Sound Test B 7" ) PORT_DIPLOCATION("SW4:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static INPUT_PORTS_START( challeng )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")  /* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x03, 0x03, "Coinage P1/P2" ) PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, "1 Credit/2 Credits" )
	PORT_DIPSETTING(    0x02, "2 Credits/3 Credits" )
	PORT_DIPSETTING(    0x01, "2 Credits/4 Credits" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW1:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW1:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW1:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW1:6" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x40, "5" )
	PORT_DIPSETTING(    0x00, "6" )

// Manual states information which differs from actual settings for DSW1
// Switches 4 & 5 are factory settings and remain in the OFF position.
// Switches 6 & 7 are factory settings which remain in the ON position.

	PORT_START("DSW1")  /* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:1,2,3")
	PORT_DIPSETTING(    0x01, "20000" )
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x07, "40000" )
	PORT_DIPSETTING(    0x06, "50000" )
	PORT_DIPSETTING(    0x05, "60000" )
	PORT_DIPSETTING(    0x04, "70000" )
	PORT_DIPSETTING(    0x03, "80000" )
	PORT_DIPSETTING(    0x02, "90000" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW2:4" )
	PORT_DIPNAME( 0x10, 0x10, "Monitor View" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "Direct" )
	PORT_DIPSETTING(    0x00, "Mirror" )
	PORT_DIPNAME( 0x20, 0x20, "Monitor Orientation" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Horizontal" )
	PORT_DIPSETTING(    0x00, "Vertical" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW2:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test A 0" ) PORT_DIPLOCATION("SW3:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test A 1" ) PORT_DIPLOCATION("SW3:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test A 2" ) PORT_DIPLOCATION("SW3:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test A 3" ) PORT_DIPLOCATION("SW3:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test A 4" ) PORT_DIPLOCATION("SW3:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test A 5" ) PORT_DIPLOCATION("SW3:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test A 6" ) PORT_DIPLOCATION("SW3:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Sound Test Enable" ) PORT_DIPLOCATION("SW3:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPNAME( 0x01, 0x00, "Sound Test B 0" ) PORT_DIPLOCATION("SW4:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Sound Test B 1" ) PORT_DIPLOCATION("SW4:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, "Sound Test B 2" ) PORT_DIPLOCATION("SW4:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Sound Test B 3" ) PORT_DIPLOCATION("SW4:4")
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Sound Test B 4" ) PORT_DIPLOCATION("SW4:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "Sound Test B 5" ) PORT_DIPLOCATION("SW4:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Sound Test B 6" ) PORT_DIPLOCATION("SW4:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Sound Test B 7" ) PORT_DIPLOCATION("SW4:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


static INPUT_PORTS_START( kaos )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("DSW0")
	PORT_DIPNAME( 0x0f, 0x0e, DEF_STR( Coinage ) ) PORT_DIPLOCATION("SW1:1,2,3,4")
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_8C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_9C ) )
	PORT_DIPSETTING(    0x05, "1 Coin/10 Credits" )
	PORT_DIPSETTING(    0x04, "1 Coin/11 Credits" )
	PORT_DIPSETTING(    0x03, "1 Coin/12 Credits" )
	PORT_DIPSETTING(    0x02, "1 Coin/13 Credits" )
	PORT_DIPSETTING(    0x01, "1 Coin/14 Credits" )
	PORT_DIPSETTING(    0x0f, DEF_STR( 2C_3C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, "Max Credits" ) PORT_DIPLOCATION("SW1:6,7")
	PORT_DIPSETTING(    0x60, "10" )
	PORT_DIPSETTING(    0x40, "20" )
	PORT_DIPSETTING(    0x20, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW1")
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x02, 0x00, "Speed" ) PORT_DIPLOCATION("SW2:2")
	PORT_DIPSETTING(    0x00, "Slow" )
	PORT_DIPSETTING(    0x02, "Fast" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:3,4")
	PORT_DIPSETTING(    0x0c, "No Bonus" )
	PORT_DIPSETTING(    0x08, "10k" )
	PORT_DIPSETTING(    0x04, "10k 30k" )
	PORT_DIPSETTING(    0x00, "10k 30k 60k" )
	PORT_DIPNAME( 0x10, 0x10, "Number of $" ) PORT_DIPLOCATION("SW2:5")
	PORT_DIPSETTING(    0x10, "8" )
	PORT_DIPSETTING(    0x00, "12" )
	PORT_DIPNAME( 0x20, 0x00, "Bonus erg" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "Every other screen" )
	PORT_DIPSETTING(    0x00, "Every screen" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW2:7" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW2:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END


static INPUT_PORTS_START( leprechn )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")  /* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x09, 0x09, DEF_STR( Coin_B ) ) PORT_DIPLOCATION("SW1:1,4")
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x22, 0x22, "Max Credits" ) PORT_DIPLOCATION("SW1:2,6")
	PORT_DIPSETTING(    0x22, "10" )
	PORT_DIPSETTING(    0x20, "20" )
	PORT_DIPSETTING(    0x02, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )

	PORT_START("DSW1")  /* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW2:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0x40, "30000" )
	PORT_DIPSETTING(    0x80, "60000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPSETTING(    0xc0, DEF_STR( None ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END


static INPUT_PORTS_START( potogold )
		PORT_INCLUDE(leprechn)
		PORT_MODIFY("DSW1")
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
INPUT_PORTS_END


static INPUT_PORTS_START( piratetr )
	PORT_START("IN0")   /* COL. A - from "TEST NO.7 - status locator - coin-door" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Do Tests") PORT_CODE(KEYCODE_F1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Select Test") PORT_CODE(KEYCODE_F2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START("IN1")   /* COL. B - from "TEST NO.7 - status locator - start sws." */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START("IN2")   /* COL. C - from "TEST NO.8 - status locator - player no.1" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )

	PORT_START("IN3")   /* COL. D - from "TEST NO.8 - status locator - player no.2" */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_COCKTAIL
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_COCKTAIL

	PORT_START("DSW0")  /* DSW A - from "TEST NO.6 - dip switch A" */
	PORT_DIPNAME( 0x09, 0x09, DEF_STR( Coin_B ) ) PORT_DIPLOCATION("SW1:1,4")
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x22, 0x22, "Max Credits" ) PORT_DIPLOCATION("SW1:2,6")
	PORT_DIPSETTING(    0x22, "10" )
	PORT_DIPSETTING(    0x20, "20" )
	PORT_DIPSETTING(    0x02, "30" )
	PORT_DIPSETTING(    0x00, "40" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Free_Play ) ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_A ) ) PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_4C ) )

	PORT_START("DSW1")  /* DSW B - from "TEST NO.6 - dip switch B" */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW2:1")
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Stringing Check" ) PORT_DIPLOCATION("SW2:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW2:3" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW2:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW2:6" )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0x40, "30000" )
	PORT_DIPSETTING(    0x80, "60000" )
	PORT_DIPSETTING(    0x00, "90000" )
	PORT_DIPSETTING(    0xc0, DEF_STR( None ) )

	PORT_START("DSW2")  /* audio board DSW A */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW3:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW3:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW3:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW3:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW3:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW3:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW3:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW3:8" )

	PORT_START("DSW3")  /* audio board DSW B */
	PORT_DIPUNUSED_DIPLOC( 0x01, 0x01, "SW4:1" )
	PORT_DIPUNUSED_DIPLOC( 0x02, 0x02, "SW4:2" )
	PORT_DIPUNUSED_DIPLOC( 0x04, 0x04, "SW4:3" )
	PORT_DIPUNUSED_DIPLOC( 0x08, 0x08, "SW4:4" )
	PORT_DIPUNUSED_DIPLOC( 0x10, 0x10, "SW4:5" )
	PORT_DIPUNUSED_DIPLOC( 0x20, 0x20, "SW4:6" )
	PORT_DIPUNUSED_DIPLOC( 0x40, 0x40, "SW4:7" )
	PORT_DIPUNUSED_DIPLOC( 0x80, 0x80, "SW4:8" )
INPUT_PORTS_END



/*************************************
 *
 *  Machine drivers
 *
 *************************************/

void gameplan_state::machine_start()
{
	/* register for save states */
	save_item(NAME(m_current_port));
	save_item(NAME(m_video_x));
	save_item(NAME(m_video_y));
	save_item(NAME(m_video_command));
	save_item(NAME(m_video_data));
	save_item(NAME(m_video_previous));
}


void gameplan_state::machine_reset()
{
	m_current_port = 0;
	m_video_x = 0;
	m_video_y = 0;
	m_video_command = 0;
	m_video_data = 0;
	m_video_previous = 0;
}

void gameplan_state::gameplan(machine_config &config)
{
	M6502(config, m_maincpu, GAMEPLAN_MAIN_CPU_CLOCK);
	m_maincpu->set_addrmap(AS_PROGRAM, &gameplan_state::gameplan_main_map);

	M6502(config, m_audiocpu, GAMEPLAN_AUDIO_CPU_CLOCK);
	m_audiocpu->set_addrmap(AS_PROGRAM, &gameplan_state::gameplan_audio_map);

	RIOT6532(config, m_riot, GAMEPLAN_AUDIO_CPU_CLOCK);
	m_riot->out_pb_callback().set(m_soundlatch, FUNC(generic_latch_8_device::write));
	m_riot->irq_callback().set(FUNC(gameplan_state::r6532_irq));

	/* video hardware */
	gameplan_video(config);

	/* audio hardware */
	SPEAKER(config, "mono").front_center();

	GENERIC_LATCH_8(config, m_soundlatch, 0);

	ay8910_device &aysnd(AY8910(config, "aysnd", GAMEPLAN_AY8910_CLOCK));
	aysnd.port_a_read_callback().set_ioport("DSW2");
	aysnd.port_b_read_callback().set_ioport("DSW3");
	aysnd.add_route(ALL_OUTPUTS, "mono", 0.33);

	/* via */
	MOS6522(config, m_via_0, GAMEPLAN_MAIN_CPU_CLOCK);
	m_via_0->writepa_handler().set(FUNC(gameplan_state::video_data_w));
	m_via_0->writepb_handler().set(FUNC(gameplan_state::gameplan_video_command_w));
	m_via_0->ca2_handler().set(FUNC(gameplan_state::video_command_trigger_w));
	m_via_0->irq_handler().set_inputline(m_maincpu, 0);

	MOS6522(config, m_via_1, GAMEPLAN_MAIN_CPU_CLOCK);
	m_via_1->readpa_handler().set(FUNC(gameplan_state::io_port_r));
	m_via_1->writepb_handler().set(FUNC(gameplan_state::io_select_w));
	m_via_1->cb2_handler().set(FUNC(gameplan_state::coin_w));

	MOS6522(config, m_via_2, GAMEPLAN_MAIN_CPU_CLOCK);
	m_via_2->readpb_handler().set(m_soundlatch, FUNC(generic_latch_8_device::read));
	m_via_2->writepa_handler().set(FUNC(gameplan_state::audio_cmd_w));
	m_via_2->ca2_handler().set(FUNC(gameplan_state::audio_trigger_w));
	m_via_2->cb2_handler().set(FUNC(gameplan_state::audio_reset_w));
}

void gameplan_state::leprechn(machine_config &config)
{
	gameplan(config);
	m_maincpu->set_clock(LEPRECHAUN_MAIN_CPU_CLOCK);
	m_via_0->set_clock(LEPRECHAUN_MAIN_CPU_CLOCK);
	m_via_1->set_clock(LEPRECHAUN_MAIN_CPU_CLOCK);
	m_via_2->set_clock(LEPRECHAUN_MAIN_CPU_CLOCK);

	/* basic machine hardware */
	m_audiocpu->set_addrmap(AS_PROGRAM, &gameplan_state::leprechn_audio_map);

	/* video hardware */
	leprechn_video(config);

	/* via */
	m_via_0->readpb_handler().set(FUNC(gameplan_state::leprechn_videoram_r));
	m_via_0->writepb_handler().set(FUNC(gameplan_state::leprechn_video_command_w));
}



/*************************************
 *
 *  ROM defintions
 *
 *************************************/

ROM_START( killcom )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "killcom.e2",   0xc000, 0x0800, CRC(a01cbb9a) SHA1(a8769243adbdddedfda5f3c8f054e9281a0eca46) )
	ROM_LOAD( "killcom.f2",   0xc800, 0x0800, CRC(bb3b4a93) SHA1(a0ea61ac30a4d191db619b7bfb697914e1500036) )
	ROM_LOAD( "killcom.g2",   0xd000, 0x0800, CRC(86ec68b2) SHA1(a09238190d61684d943ce0acda25eb901d2580ac) )
	ROM_LOAD( "killcom.j2",   0xd800, 0x0800, CRC(28d8c6a1) SHA1(d9003410a651221e608c0dd20d4c9c60c3b0febc) )
	ROM_LOAD( "killcom.j1",   0xe000, 0x0800, CRC(33ef5ac5) SHA1(42f839ad295d3df457ced7886a0003eff7e6c4ae) )
	ROM_LOAD( "killcom.g1",   0xe800, 0x0800, CRC(49cb13e2) SHA1(635e4665042ddd9b8c0b9f275d4bcc6830dc6a98) )
	ROM_LOAD( "killcom.f1",   0xf000, 0x0800, CRC(ef652762) SHA1(414714e5a3f83916bd3ae54afe2cb2271ee9008b) )
	ROM_LOAD( "killcom.e1",   0xf800, 0x0800, CRC(bc19dcb7) SHA1(eb983d2df010c12cb3ffb584fceafa54a4e956b3) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "killsnd.e1",   0xe000, 0x0800, CRC(77d4890d) SHA1(a3ed7e11dec5d404f022c521256ff50aa6940d3c) )
ROM_END

ROM_START( megatack )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "megattac.e2",  0xc000, 0x0800, CRC(33fa5104) SHA1(15693eb540563e03502b53ed8a83366e395ca529) )
	ROM_LOAD( "megattac.f2",  0xc800, 0x0800, CRC(af5e96b1) SHA1(5f6ab47c12d051f6af446b08f3cd459fbd2c13bf) )
	ROM_LOAD( "megattac.g2",  0xd000, 0x0800, CRC(670103ea) SHA1(e11f01e8843ed918c6ea5dda75319dc95105d345) )
	ROM_LOAD( "megattac.j2",  0xd800, 0x0800, CRC(4573b798) SHA1(388db11ab114b3575fe26ed65bbf49174021939a) )
	ROM_LOAD( "megattac.j1",  0xe000, 0x0800, CRC(3b1d01a1) SHA1(30bbf51885b1e510b8d21cdd82244a455c5dada0) )
	ROM_LOAD( "megattac.g1",  0xe800, 0x0800, CRC(eed75ef4) SHA1(7c02337344f2716d2f2771229f7dee7b651eeb95) )
	ROM_LOAD( "megattac.f1",  0xf000, 0x0800, CRC(c93a8ed4) SHA1(c87e2f13f2cc00055f4941c272a3126b165a6252) )
	ROM_LOAD( "megattac.e1",  0xf800, 0x0800, CRC(d9996b9f) SHA1(e71d65b695000fdfd5fd1ce9ae39c0cb0b61669e) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "megatsnd.e1",  0xe000, 0x0800, CRC(0c186bdb) SHA1(233af9481a3979971f2d5aa75ec8df4333aa5e0d) )
ROM_END

ROM_START( megatacka ) // original Centuri PCB
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "meg-e2.bin",  0xc000, 0x0800, CRC(9664c7b1) SHA1(356e7f5f3b2a9b829fac53e7bf9193278b4de2ed) )
	ROM_LOAD( "meg-f2.bin",  0xc800, 0x0800, CRC(67c42523) SHA1(f9fc88cdea05a2d0e89e3ba9b545bf3476b37d2d) )
	ROM_LOAD( "meg-g2.bin",  0xd000, 0x0800, CRC(71f36604) SHA1(043988126343b6224e8e1d6c0dbba6b6b08fe493) )
	ROM_LOAD( "meg-j2.bin",  0xd800, 0x0800, CRC(4ddcc145) SHA1(3a6d42a58c388eaaf6561351fa98936d98975e0b) )
	ROM_LOAD( "meg-j1.bin",  0xe000, 0x0800, CRC(911d5d9a) SHA1(92bfe0f69a6e563363df59ebee745d7b3cfc0141) )
	ROM_LOAD( "meg-g1.bin",  0xe800, 0x0800, CRC(22a51c9b) SHA1(556e09216ed85eaf3870f85515c273c7eb1ab13a) )
	ROM_LOAD( "meg-f1.bin",  0xf000, 0x0800, CRC(2ffa51ac) SHA1(7c5d8295c5e71a9918a02d203139b024bd3bf8f4) )
	ROM_LOAD( "meg-e1.bin",  0xf800, 0x0800, CRC(01dbe4ad) SHA1(af72778ae112f24a92fb3007bb456331c3896b50) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "megatsnd.e1",  0xe000, 0x0800, CRC(0c186bdb) SHA1(233af9481a3979971f2d5aa75ec8df4333aa5e0d) ) // missing for this board, using the one from the parent
ROM_END

ROM_START( challeng )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "chall.6",      0xa000, 0x1000, CRC(b30fe7f5) SHA1(ce93a57d626f90d31ddedbc35135f70758949dfa) )
	ROM_LOAD( "chall.5",      0xb000, 0x1000, CRC(34c6a88e) SHA1(250577e2c8eb1d3a78cac679310ec38924ac1fe0) )
	ROM_LOAD( "chall.4",      0xc000, 0x1000, CRC(0ddc18ef) SHA1(9f1aa27c71d7e7533bddf7de420c06fb0058cf60) )
	ROM_LOAD( "chall.3",      0xd000, 0x1000, CRC(6ce03312) SHA1(69c047f501f327f568fe4ad1274168f9dda1ca70) )
	ROM_LOAD( "chall.2",      0xe000, 0x1000, CRC(948912ad) SHA1(e79738ab94501f858f4d5f218787267523611e92) )
	ROM_LOAD( "chall.1",      0xf000, 0x1000, CRC(7c71a9dc) SHA1(2530ada6390fb46c44bf7bf2636910ee54786493) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "chall.snd",    0xe000, 0x0800, CRC(1b2bffd2) SHA1(36ceb5abbc92a17576c375019f1c5900320398f9) )
ROM_END

ROM_START( kaos )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "kaosab.g2",    0x9000, 0x0800, CRC(b23d858f) SHA1(e31fa657ace34130211a0b9fc0d115fd89bb20dd) )
	ROM_CONTINUE(             0xd000, 0x0800 )
	ROM_LOAD( "kaosab.j2",    0x9800, 0x0800, CRC(4861e5dc) SHA1(96ca0b8625af3897bd4a50a45ea964715f9e4973) )
	ROM_CONTINUE(             0xd800, 0x0800 )
	ROM_LOAD( "kaosab.j1",    0xa000, 0x0800, CRC(e055db3f) SHA1(099176629723c1a9bdc59f440339b2e8c38c3261) )
	ROM_CONTINUE(             0xe000, 0x0800 )
	ROM_LOAD( "kaosab.g1",    0xa800, 0x0800, CRC(35d7c467) SHA1(6d5bfd29ff7b96fed4b24c899ddd380e47e52bc5) )
	ROM_CONTINUE(             0xe800, 0x0800 )
	ROM_LOAD( "kaosab.f1",    0xb000, 0x0800, CRC(995b9260) SHA1(580896aa8b6f0618dc532a12d0795b0d03f7cadd) )
	ROM_CONTINUE(             0xf000, 0x0800 )
	ROM_LOAD( "kaosab.e1",    0xb800, 0x0800, CRC(3da5202a) SHA1(6b5aaf44377415763aa0895c64765a4b82086f25) )
	ROM_CONTINUE(             0xf800, 0x0800 )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "kaossnd.e1",   0xe000, 0x0800, CRC(ab23d52a) SHA1(505f3e4a56e78a3913010f5484891f01c9831480) )
ROM_END

ROM_START( leprechn )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "lep1",         0x8000, 0x1000, CRC(2c4a46ca) SHA1(28a157c1514bc9f27cc27baddb83cf1a1887f3d1) )
	ROM_LOAD( "lep2",         0x9000, 0x1000, CRC(6ed26b3e) SHA1(4ee5d09200d9e8f94ae29751c8ee838faa268f15) )
	ROM_LOAD( "lep3",         0xa000, 0x1000, CRC(a2eaa016) SHA1(be992ee787766137fd800ec59529c98ef2e6991e) )
	ROM_LOAD( "lep4",         0xb000, 0x1000, CRC(6c12a065) SHA1(2acae6a5b94cbdcc550cee88a7be9254fdae908c) )
	ROM_LOAD( "lep5",         0xc000, 0x1000, CRC(21ddb539) SHA1(b4dd0a1916adc076fa6084c315459fcb2522161e) )
	ROM_LOAD( "lep6",         0xd000, 0x1000, CRC(03c34dce) SHA1(6dff202e1a3d0643050f3287f6b5906613d56511) )
	ROM_LOAD( "lep7",         0xe000, 0x1000, CRC(7e06d56d) SHA1(5f68f2047969d803b752a4cd02e0e0af916c8358) )
	ROM_LOAD( "lep8",         0xf000, 0x1000, CRC(097ede60) SHA1(5509c41167c066fa4e7f4f4bd1ce9cd00773a82c) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "lepsound",     0xe000, 0x1000, CRC(6651e294) SHA1(ce2875fc4df61a30d51d3bf2153864b562601151) )
ROM_END

ROM_START( potogold )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "pog.pg1",      0x8000, 0x1000, CRC(9f1dbda6) SHA1(baf20e9a0793c0f1529396f95a820bd1f9431465) )
	ROM_LOAD( "pog.pg2",      0x9000, 0x1000, CRC(a70e3811) SHA1(7ee306dc7d75a7d3fd497870ec92bef9d86535e9) )
	ROM_LOAD( "pog.pg3",      0xa000, 0x1000, CRC(81cfb516) SHA1(12732707e2a51ec39563f2d1e898cc567ab688f0) )
	ROM_LOAD( "pog.pg4",      0xb000, 0x1000, CRC(d61b1f33) SHA1(da024c0776214b8b5a3e49401c4110e86a1bead1) )
	ROM_LOAD( "pog.pg5",      0xc000, 0x1000, CRC(eee7597e) SHA1(9b5cd293580c5d212f8bf39286070280d55e4cb3) )
	ROM_LOAD( "pog.pg6",      0xd000, 0x1000, CRC(25e682bc) SHA1(085d2d553ec10f2f830918df3a7fb8e8c1e5d18c) )
	ROM_LOAD( "pog.pg7",      0xe000, 0x1000, CRC(84399f54) SHA1(c90ba3e3120adda2785ab5abd309e0a703d39f8b) )
	ROM_LOAD( "pog.pg8",      0xf000, 0x1000, CRC(9e995a1a) SHA1(5c525e6c161d9d7d646857b27cecfbf8e0943480) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "pog.snd",      0xe000, 0x1000, CRC(ec61f0a4) SHA1(26944ecc3e7413259928c8b0a74b2260e67d2c4e) )
ROM_END

ROM_START( leprechp )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "lep1",         0x8000, 0x1000, CRC(2c4a46ca) SHA1(28a157c1514bc9f27cc27baddb83cf1a1887f3d1) )
	ROM_LOAD( "lep2",         0x9000, 0x1000, CRC(6ed26b3e) SHA1(4ee5d09200d9e8f94ae29751c8ee838faa268f15) )
	ROM_LOAD( "3u15.bin",     0xa000, 0x1000, CRC(b5f79fd8) SHA1(271f7b55ecda5bb99f40687264256b82649e2141) )
	ROM_LOAD( "lep4",         0xb000, 0x1000, CRC(6c12a065) SHA1(2acae6a5b94cbdcc550cee88a7be9254fdae908c) )
	ROM_LOAD( "lep5",         0xc000, 0x1000, CRC(21ddb539) SHA1(b4dd0a1916adc076fa6084c315459fcb2522161e) )
	ROM_LOAD( "lep6",         0xd000, 0x1000, CRC(03c34dce) SHA1(6dff202e1a3d0643050f3287f6b5906613d56511) )
	ROM_LOAD( "lep7",         0xe000, 0x1000, CRC(7e06d56d) SHA1(5f68f2047969d803b752a4cd02e0e0af916c8358) )
	ROM_LOAD( "lep8",         0xf000, 0x1000, CRC(097ede60) SHA1(5509c41167c066fa4e7f4f4bd1ce9cd00773a82c) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "lepsound",     0xe000, 0x1000, CRC(6651e294) SHA1(ce2875fc4df61a30d51d3bf2153864b562601151) )
ROM_END

ROM_START( piratetr )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "1u13.bin",     0x8000, 0x1000, CRC(4433bb61) SHA1(eee0d7f356118f8595dd7533541db744a63a8176) )
	ROM_LOAD( "2u14.bin",     0x9000, 0x1000, CRC(9bdc4b77) SHA1(ebaab8b3024efd3d0b76647085d441ca204ad5d5) )
	ROM_LOAD( "3u15.bin",     0xa000, 0x1000, CRC(ebced718) SHA1(3a2f4385347f14093360cfa595922254c9badf1a) )
	ROM_LOAD( "4u16.bin",     0xb000, 0x1000, CRC(f494e657) SHA1(83a31849de8f4f70d7547199f229079f491ddc61) )
	ROM_LOAD( "5u17.bin",     0xc000, 0x1000, CRC(2789d68e) SHA1(af8f334ce4938cd75143b729c97cfbefd68c9e13) )
	ROM_LOAD( "6u18.bin",     0xd000, 0x1000, CRC(d91abb3a) SHA1(11170e69686c2a1f2dc31d41516f44b612f99bad) )
	ROM_LOAD( "7u19.bin",     0xe000, 0x1000, CRC(6e8808c4) SHA1(d1f76fd37d8f78552a9d53467073cc9a571d96ce) )
	ROM_LOAD( "8u20.bin",     0xf000, 0x1000, CRC(2802d626) SHA1(b0db688500076ee73e0001c00089a8d552c6f607) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "su31.bin",     0xe000, 0x1000, CRC(2fe86a11) SHA1(aaafe411b9cb3d0221cc2af73d34ad8bb74f8327) )
ROM_END



/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1980, killcom,   0,        gameplan, killcom,  gameplan_state, empty_init, ROT0,   "Game Plan (Centuri license)",                     "Killer Comet",         MACHINE_SUPPORTS_SAVE )
GAME( 1980, megatack,  0,        gameplan, megatack, gameplan_state, empty_init, ROT0,   "Game Plan (Centuri license)",                     "Megatack (set 1)",     MACHINE_SUPPORTS_SAVE )
GAME( 1980, megatacka, megatack, gameplan, megatack, gameplan_state, empty_init, ROT0,   "Game Plan (Centuri license)",                     "Megatack (set 2)",     MACHINE_SUPPORTS_SAVE )
GAME( 1981, challeng,  0,        gameplan, challeng, gameplan_state, empty_init, ROT0,   "Game Plan (Centuri license)",                     "Challenger",           MACHINE_SUPPORTS_SAVE )
GAME( 1981, kaos,      0,        gameplan, kaos,     gameplan_state, empty_init, ROT270, "Game Plan",                                       "Kaos",                 MACHINE_SUPPORTS_SAVE )
GAME( 1982, leprechn,  0,        leprechn, leprechn, gameplan_state, empty_init, ROT0,   "Tong Electronic",                                 "Leprechaun",           MACHINE_SUPPORTS_SAVE )
GAME( 1982, potogold,  leprechn, leprechn, potogold, gameplan_state, empty_init, ROT0,   "Tong Electronic (Game Plan license)",             "Pot of Gold",          MACHINE_SUPPORTS_SAVE )
GAME( 1982, leprechp,  leprechn, leprechn, potogold, gameplan_state, empty_init, ROT0,   "Tong Electronic (Pacific Polytechnical license)", "Leprechaun (Pacific)", MACHINE_SUPPORTS_SAVE )
GAME( 1982, piratetr,  0,        leprechn, piratetr, gameplan_state, empty_init, ROT0,   "Tong Electronic",                                 "Pirate Treasure",      MACHINE_SUPPORTS_SAVE )
