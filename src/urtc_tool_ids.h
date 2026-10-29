/* GENERATED from spec/tools.json by tools/generate_tools.py - do not edit by hand.
 * Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
 * GPL-3.0-or-later - see LICENSE
 *
 * The five-bit tool identifier of a tool head (its ID jumpers, or the byte stored in its F-RAM): 0 .. URTC_TOOL_ID_LAST are tool profiles, the values
 * after them up to 30 are not assigned to any tool, and 31 means a free configuration (the tool is chosen by a value stored in the head).
 */
#ifndef URTC_TOOL_IDS_H
#define URTC_TOOL_IDS_H

#define URTC_TOOL_ID_SOLDERING_IRON 0u
#define URTC_TOOL_ID_PASTE_DISPENSER 1u
#define URTC_TOOL_ID_LIQUID_DISPENSER 2u
#define URTC_TOOL_ID_SCREWDRIVER 3u
#define URTC_TOOL_ID_VACUUM_PICKUP 4u
#define URTC_TOOL_ID_DRILL 5u
#define URTC_TOOL_ID_GRIPPER_GIMBAL 6u
#define URTC_TOOL_ID_GRIPPER_NEMA 7u
#define URTC_TOOL_ID_AOI_INSPECTION 8u
#define URTC_TOOL_ID_LASER_ENGRAVER 9u
#define URTC_TOOL_ID_PRINTER_3D 10u
#define URTC_TOOL_ID_SCAN_PROBE 11u
#define URTC_TOOL_ID_SMT_PICKPLACE 12u
#define URTC_TOOL_ID_ELECTROMAGNET 13u
#define URTC_TOOL_ID_SPOT_WELDER 14u
#define URTC_TOOL_ID_CONFORMAL_COATING 15u
#define URTC_TOOL_ID_VACUUM_GRIPPER_LG 16u
#define URTC_TOOL_ID_FLYING_PROBE 17u
#define URTC_TOOL_ID_UV_CURING 18u
#define URTC_TOOL_ID_HOTAIR_REWORK 19u
#define URTC_TOOL_ID_PRESSFIT_INSERTER 20u
#define URTC_TOOL_ID_CRIMPING_ACTUATOR 21u
#define URTC_TOOL_ID_THERMAL_INSPECTION 22u
#define URTC_TOOL_ID_PASTE_JETTING 23u
#define URTC_TOOL_ID_ULTRASONIC_WELDER 24u

#define URTC_TOOL_ID_COUNT 25u
#define URTC_TOOL_ID_LAST 24u
#define URTC_TOOL_ID_FIRST_INVALID 25u
#define URTC_TOOL_ID_FREE_CONFIG 31u
#define URTC_TOOL_ID_MASK 0x1Fu

#endif /* URTC_TOOL_IDS_H */
