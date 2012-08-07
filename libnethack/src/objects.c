/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) Mike Threepoint, 1989.				  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef OBJECTS_PASS_2_
/* first pass */
struct monst {
    struct monst *dummy;
};      /* lint: struct obj's union */

# include "config.h"
# include "obj.h"
# include "objclass.h"
# include "prop.h"
# include "skills.h" 
#else /* !OBJECTS_PASS_2_ */
/* second pass */
# include "color.h"
# define COLOR_FIELD(X) X,
#endif /* !OBJECTS_PASS_2_ */


/* objects have symbols: ) [ = " ( % ! ? + / $ * ` 0 _ . */

/*
 *	Note:  OBJ() and BITS() macros are used to avoid exceeding argument
 *	limits imposed by some compilers.  The ctnr field of BITS currently
 *	does not map into struct objclass, and is ignored in the expansion.
 *	The 0 in the expansion corresponds to oc_pre_discovered, which is
 *	set at run-time during role-specific character initialization.
 */

#ifndef OBJECTS_PASS_2_
/* first pass -- object descriptive text */

/* Names are always nouns; descriptions/appearences always adjectives, with the
   exception of scroll labels, which are nouns */
# define OBJ(name,desc) name,desc
# define OBJECT(obj,bits,prp,sym,prob,dly,wt,cost,sdam,ldam,oc1,oc2,nut,color) \
	{obj}

const struct objdescr obj_descr[] = {
#else
/* second pass -- object definitions */

# define BITS(nmkn,mrg,uskn,ctnr,mgc,chrg,uniq,nwsh,big,tuf,dir,sub,mtrl) \
        nmkn,mrg,uskn,0,mgc,chrg,uniq,nwsh,0,big,tuf,dir,mtrl,sub
# define OBJECT(obj,bits,prp,sym,prob,dly,wt,cost,sdam,ldam,oc1,oc2,nut,color) \
	{0, 0, NULL, bits, prp, sym, dly, COLOR_FIELD(color) \
	 prob, wt, cost, sdam, ldam, oc1, oc2, nut}
# define HARDGEM(n) (n >= 8)

struct objclass *objects;

const struct objclass const_objects[] = {
#endif
/* dummy object[0] -- description [2nd arg] *must* be NULL */
    OBJECT(OBJ("N{N{object},A{strange}}", NULL),
           BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, 0),
           0, ILLOBJ_CLASS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),

/* weapons ... */
#define WEAPON(name,app,kn,mg,bi,prob,wt,cost,sdam,ldam,hitbon,typ,sub,metal,color) \
	OBJECT( \
		OBJ(name,app), BITS(kn,mg,1,0,0,1,0,0,bi,0,typ,sub,metal), 0, \
		WEAPON_CLASS, prob, 0, \
		wt, cost, sdam, ldam, hitbon, 0, wt, color )
#define PROJECTILE(name,app,kn,prob,wt,cost,sdam,ldam,hitbon,metal,sub,color) \
	OBJECT( \
		OBJ(name,app), \
		BITS(kn,1,1,0,0,1,0,0,0,0,PIERCE,sub,metal), 0, \
		WEAPON_CLASS, prob, 0, \
		wt, cost, sdam, ldam, hitbon, 0, wt, color )
#define BOW(name,app,kn,prob,wt,cost,hitbon,metal,sub,color) \
	OBJECT( \
		OBJ(name,app), BITS(kn,0,1,0,0,1,0,0,0,0,0,sub,metal), 0, \
		WEAPON_CLASS, prob, 0, \
		wt, cost, 2, 2, hitbon, 0, wt, color )

/* Note: for weapons that don't do an even die of damage (ex. 2-7 or 3-18)
 * the extra damage is added on in weapon.c, not here! */

#define P PIERCE
#define S SLASH
#define B WHACK

/* missiles */
    PROJECTILE("N{arrow}", NULL,
               1, 55, 1, 2, 6, 6, 0, IRON, -P_BOW, HI_METAL),
    PROJECTILE("N{N{arrow},A{elven}}", "A{runed}",
               0, 20, 1, 2, 7, 6, 0, WOOD, -P_BOW, HI_WOOD),
    PROJECTILE("N{N{arrow},A{orcish}}", "A{crude}",
               0, 20, 1, 2, 5, 6, 0, IRON, -P_BOW, CLR_BLACK),
    PROJECTILE("N{N{arrow},A{silver}}", NULL,
               1, 12, 1, 5, 6, 6, 0, SILVER, -P_BOW, HI_SILVER),
    PROJECTILE("N{ya}", "A{bamboo}",
               0, 15, 1, 4, 7, 7, 1, METAL, -P_BOW, HI_METAL),
    PROJECTILE("N{crossbow bolt}", NULL,
               1, 55, 1, 2, 4, 6, 0, IRON, -P_CROSSBOW, HI_METAL),

    WEAPON("N{dart}", NULL,
           1, 1, 0, 60, 1, 2, 3, 2, 0, P, -P_DART, IRON, HI_METAL),
    WEAPON("N{shuriken}", NULL,
           0, 1, 0, 35, 1, 5, 8, 6, 2, P, -P_SHURIKEN, IRON, HI_METAL),
    WEAPON("N{boomerang}", NULL,
           1, 1, 0, 15, 5, 20, 9, 9, 0, 0, -P_BOOMERANG, WOOD, HI_WOOD),

/* spears */
    WEAPON("N{spear}", NULL,
           1, 1, 0, 50, 30, 3, 6, 8, 0, P, P_SPEAR, IRON, HI_METAL),
    WEAPON("N{N{spear},A{elven}}", "A{runed}",
           0, 1, 0, 10, 30, 3, 7, 8, 0, P, P_SPEAR, WOOD, HI_WOOD),
    WEAPON("N{N{spear},A{orcish}}", "A{crude}",
           0, 1, 0, 13, 30, 3, 5, 8, 0, P, P_SPEAR, IRON, CLR_BLACK),
    WEAPON("N{N{spear},A{dwarvish}}", "A{stout}",
           0, 1, 0, 12, 35, 3, 8, 8, 0, P, P_SPEAR, IRON, HI_METAL),
    WEAPON("N{N{spear},A{silver}}", NULL,
           1, 1, 0, 2, 36, 40, 6, 8, 0, P, P_SPEAR, SILVER, HI_SILVER),
    WEAPON("N{javelin}", NULL,
           0, 1, 0, 10, 20, 3, 6, 6, 0, P, P_JAVELIN, IRON, HI_METAL),

    WEAPON("N{trident}", NULL,
           1, 0, 0, 8, 25, 5, 6, 4, 0, P, P_TRIDENT, IRON, HI_METAL),
    /* +1 small, +2d4 large */

/* blades */
    WEAPON("N{dagger}", NULL,
           1, 1, 0, 30, 10, 4, 4, 3, 2, P, P_DAGGER, IRON, HI_METAL),
    WEAPON("N{N{dagger},A{elven}}", "A{runed}",
           0, 1, 0, 10, 10, 4, 5, 3, 2, P, P_DAGGER, WOOD, HI_WOOD),
    WEAPON("N{N{dagger},A{orcish}}", "A{crude}",
           0, 1, 0, 12, 10, 4, 3, 3, 2, P, P_DAGGER, IRON, CLR_BLACK),
    WEAPON("N{N{dagger},A{silver}}", NULL,
           1, 1, 0, 3, 12, 40, 4, 3, 2, P, P_DAGGER, SILVER, HI_SILVER),
    WEAPON("N{athame}", NULL,
           1, 1, 0, 0, 10, 4, 4, 3, 2, S, P_DAGGER, IRON, HI_METAL),
    WEAPON("N{scalpel}", NULL,
           1, 1, 0, 0, 5, 6, 3, 3, 2, S, P_KNIFE, METAL, HI_METAL),
    WEAPON("N{knife}", NULL,
           1, 1, 0, 20, 5, 4, 3, 2, 0, P | S, P_KNIFE, IRON, HI_METAL),
    WEAPON("N{stiletto}", NULL,
           1, 1, 0, 5, 5, 4, 3, 2, 0, P | S, P_KNIFE, IRON, HI_METAL),
    WEAPON("N{N{tooth},A{N{worm}}}", NULL,
           1, 0, 0, 0, 20, 2, 2, 2, 0, 0, P_KNIFE, 0, CLR_WHITE),
    WEAPON("N{crysknife}", NULL,
           1, 0, 0, 0, 20, 100, 10, 10, 3, P, P_KNIFE, MINERAL, CLR_WHITE),

    WEAPON("N{axe}", NULL,
           1, 0, 0, 40, 60, 8, 6, 4, 0, S, P_AXE, IRON, HI_METAL),
    WEAPON("N{N{axe},A{N{battle}}}", "A{double-headed}",
           0, 0, 1, 10, 120, 40, 8, 6, 0, S, P_AXE, IRON, HI_METAL),
    /* 'double-bitted' ? */

/* swords */
    WEAPON("N{N{sword},A{short}}", NULL,
           1, 0, 0, 8, 30, 10, 6, 8, 0, P, P_SHORT_SWORD, IRON, HI_METAL),
    WEAPON("N{N{N{sword},A{short}},A{elven}}", "A{runed}",
           0, 0, 0, 2, 30, 10, 8, 8, 0, P, P_SHORT_SWORD, WOOD, HI_WOOD),
    WEAPON("N{N{N{sword},A{short}},A{orcish}}", "A{crude}",
           0, 0, 0, 3, 30, 10, 5, 8, 0, P, P_SHORT_SWORD, IRON, CLR_BLACK),
    WEAPON("N{N{N{sword},A{short}},A{dwarvish}}", "A{broad}",
           0, 0, 0, 2, 30, 10, 7, 8, 0, P, P_SHORT_SWORD, IRON, HI_METAL),
    WEAPON("N{scimitar}", NULL,
           0, 0, 0, 15, 40, 15, 8, 8, 0, S, P_SCIMITAR, IRON, HI_METAL),
    WEAPON("N{N{saber},A{silver}}", NULL,
           1, 0, 0, 6, 40, 75, 8, 8, 0, S, P_SABER, SILVER, HI_SILVER),
    WEAPON("N{broadsword}", NULL,
           1, 0, 0, 8, 70, 10, 4, 6, 0, S, P_BROAD_SWORD, IRON, HI_METAL),
    /* +d4 small, +1 large */
    WEAPON("N{N{broadsword},A{elven}}", "A{runed}",
           0, 0, 0, 4, 70, 10, 6, 6, 0, S, P_BROAD_SWORD, WOOD, HI_WOOD),
    /* +d4 small, +1 large */
    WEAPON("N{N{sword},A{long}}", NULL,
           1, 0, 0, 50, 40, 15, 8, 12, 0, S, P_LONG_SWORD, IRON, HI_METAL),
    WEAPON("N{N{sword},A{two-handed}}", NULL,
           1, 0, 1, 22, 150, 50, 12, 6, 0, S, P_TWO_HANDED_SWORD, IRON,
           HI_METAL),
    /* +2d6 large */
    WEAPON("N{katana}", "A{samurai}",
           0, 0, 0, 4, 40, 80, 10, 12, 1, S, P_LONG_SWORD, IRON, HI_METAL),
/* special swords set up for artifacts */
    WEAPON("N{tsurugi}", NULL,
           0, 0, 1, 0, 60, 500, 16, 8, 2, S, P_TWO_HANDED_SWORD, METAL,
           HI_METAL),
    /* +2d6 large */
    WEAPON("N{runesword}", "A{runed}",
           0, 0, 0, 0, 40, 300, 4, 6, 0, S, P_BROAD_SWORD, IRON, CLR_BLACK),
    /* +d4 small, +1 large */
    /* +5d2 +d8 from level drain */

/* polearms */
/* spear-type */
    WEAPON("N{partisan}", "A{vulgar}",
           0, 0, 1, 5, 80, 10, 6, 6, 0, P, P_POLEARMS, IRON, HI_METAL),
    /* +1 large */
    WEAPON("N{ranseur}", "A{hilted}",
           0, 0, 1, 5, 50, 6, 4, 4, 0, P, P_POLEARMS, IRON, HI_METAL),
    /* +d4 both */
    WEAPON("N{spetum}", "A{forked}",
           0, 0, 1, 5, 50, 5, 6, 6, 0, P, P_POLEARMS, IRON, HI_METAL),
    /* +1 small, +d6 large */
    WEAPON("N{glaive}", "A{single-edged}",
           0, 0, 1, 8, 75, 6, 6, 10, 0, S, P_POLEARMS, IRON, HI_METAL),
    WEAPON("N{lance}", NULL,
           1, 0, 0, 4, 180, 10, 6, 8, 0, P, P_LANCE, IRON, HI_METAL),
/* axe-type */
    WEAPON("N{halberd}", "A{angled}",
           0, 0, 1, 8, 150, 10, 10, 6, 0, P | S, P_POLEARMS, IRON, HI_METAL),
    /* +1d6 large */
    WEAPON("N{bardiche}", "A{long}",
           0, 0, 1, 4, 120, 7, 4, 4, 0, S, P_POLEARMS, IRON, HI_METAL),
    /* +1d4 small, +2d4 large */
    WEAPON("N{voulge}", "A{cleaving}",
           0, 0, 1, 4, 125, 5, 4, 4, 0, S, P_POLEARMS, IRON, HI_METAL),
    /* +d4 both */
    WEAPON("N{N{mattock},A{dwarvish}}", "A{broad}",
           0, 0, 1, 13, 120, 50, 12, 8, -1, B, P_PICK_AXE, IRON, HI_METAL),

/* curved/hooked */
    WEAPON("N{fauchard}", "A{curved}",
           0, 0, 1, 6, 60, 5, 6, 8, 0, P | S, P_POLEARMS, IRON, HI_METAL),
    WEAPON("N{guisarme}", "A{pruning}",
           0, 0, 1, 6, 80, 5, 4, 8, 0, S, P_POLEARMS, IRON, HI_METAL),
    /* +1d4 small */
    WEAPON("N{bill-guisarme}", "A{hooked}",
           0, 0, 1, 4, 120, 7, 4, 10, 0, P | S, P_POLEARMS, IRON, HI_METAL),
    /* +1d4 small */
/* other */
    WEAPON("N{lucern hammer}", "A{pronged}",
           0, 0, 1, 5, 150, 7, 4, 6, 0, B | P, P_POLEARMS, IRON, HI_METAL),
    /* +1d4 small */
    WEAPON("N{bec de corbin}", "A{beaked}",
           0, 0, 1, 4, 100, 8, 8, 6, 0, B | P, P_POLEARMS, IRON, HI_METAL),

/* bludgeons */
    WEAPON("N{mace}", NULL,
           1, 0, 0, 40, 30, 5, 6, 6, 0, B, P_MACE, IRON, HI_METAL),
    /* +1 small */
    WEAPON("N{morning star}", NULL,
           1, 0, 0, 12, 120, 10, 4, 6, 0, B, P_MORNING_STAR, IRON, HI_METAL),
    /* +d4 small, +1 large */
    WEAPON("N{war hammer}", NULL,
           1, 0, 0, 15, 50, 5, 4, 4, 0, B, P_HAMMER, IRON, HI_METAL),
    /* +1 small */
    WEAPON("N{club}", NULL,
           1, 0, 0, 12, 30, 3, 6, 3, 0, B, P_CLUB, WOOD, HI_WOOD),
    WEAPON("N{rubber hose}", NULL,
           1, 0, 0, 0, 20, 3, 4, 3, 0, B, P_WHIP, PLASTIC, CLR_BROWN),
    WEAPON("N{quarterstaff}", NULL,
           0, 0, 1, 11, 40, 5, 6, 6, 0, B, P_QUARTERSTAFF, WOOD, HI_WOOD),
/* two-piece */
    WEAPON("N{aklys}", "A{thonged}",
           0, 0, 0, 8, 15, 4, 6, 3, 0, B, P_CLUB, IRON, HI_METAL),
    WEAPON("N{flail}", NULL,
           1, 0, 0, 40, 15, 4, 6, 4, 0, B, P_FLAIL, IRON, HI_METAL),
    /* +1 small, +1d4 large */
/* misc */
    WEAPON("N{bullwhip}", NULL,
           1, 0, 0, 2, 20, 4, 2, 1, 0, 0, P_WHIP, LEATHER, CLR_BROWN),

/* bows */
    BOW("N{bow}", NULL, 1, 24, 30, 60, 0, WOOD, P_BOW, HI_WOOD),
    BOW("N{N{bow},A{elven}}", "A{runed}", 0, 12, 30, 60, 0, WOOD, P_BOW, HI_WOOD),
    BOW("N{N{bow},A{orcish}}", "A{crude}", 0, 12, 30, 60, 0, WOOD, P_BOW, CLR_BLACK),
    BOW("N{yumi}", "A{long}", 0, 0, 30, 60, 0, WOOD, P_BOW, HI_WOOD),
    BOW("N{sling}", NULL, 1, 40, 3, 20, 0, LEATHER, P_SLING, HI_LEATHER),
    BOW("N{crossbow}", NULL, 1, 45, 50, 40, 0, WOOD, P_CROSSBOW, HI_WOOD),

#undef P
#undef S
#undef B

#undef WEAPON
#undef PROJECTILE
#undef BOW

/* armor ... */
/* IRON denotes ferrous metals, including steel.
 * Only IRON weapons and armor can rust.
 * Only COPPER (including brass) corrodes.
 * Some creatures are vulnerable to SILVER.
 */
#define ARMOR(name,desc,kn,mgc,blk,power,prob,delay,wt,cost,ac,can,sub,metal,c) \
	OBJECT( \
		OBJ(name,desc), BITS(kn,0,1,0,mgc,1,0,0,blk,0,0,sub,metal), power, \
		ARMOR_CLASS, prob, delay, wt, cost, \
		0, 0, 10 - ac, can, wt, c )
#define HELM(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c) \
	ARMOR(name,desc,kn,mgc,0,power,prob,delay,wt,cost,ac,can,ARM_HELM,metal,c)
#define CLOAK(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c) \
	ARMOR(name,desc,kn,mgc,0,power,prob,delay,wt,cost,ac,can,ARM_CLOAK,metal,c)
#define SHIELD(name,desc,kn,mgc,blk,power,prob,delay,wt,cost,ac,can,metal,c) \
	ARMOR(name,desc,kn,mgc,blk,power,prob,delay,wt,cost,ac,can,ARM_SHIELD,metal,c)
#define GLOVES(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c) \
	ARMOR(name,desc,kn,mgc,0,power,prob,delay,wt,cost,ac,can,ARM_GLOVES,metal,c)
#define BOOTS(name,desc,kn,mgc,power,prob,delay,wt,cost,ac,can,metal,c) \
	ARMOR(name,desc,kn,mgc,0,power,prob,delay,wt,cost,ac,can,ARM_BOOTS,metal,c)

/* helmets */
    HELM("N{N{N{helm},A{leather}},A{elven}}", "A{leather}",
         0, 0, 0, 6, 1, 3, 8, 9, 0, LEATHER, HI_LEATHER),
    HELM("N{N{helm},A{orcish}}", "N{iron}",
         0, 0, 0, 6, 1, 30, 10, 9, 0, IRON, CLR_BLACK),
    HELM("N{N{N{helm},A{iron}},A{dwarvish}}", "A{hard}",
         0, 0, 0, 6, 1, 40, 20, 8, 0, IRON, HI_METAL),
    HELM("N{fedora}", NULL,
         1, 0, 0, 0, 0, 3, 1, 10, 0, CLOTH, CLR_BROWN),
    HELM("N{cornuthaum}", "A{conical}",
         0, 1, CLAIRVOYANT,
         3, 1, 4, 80, 10, 2, CLOTH, CLR_BLUE),
    HELM("N{N{cap},A{N{dunce}}}", "A{conical}",
         0, 1, 0, 3, 1, 4, 1, 10, 0, CLOTH, CLR_BLUE),
    HELM("N{N{pot},A{dented}}", NULL,
         1, 0, 0, 2, 0, 10, 8, 9, 0, IRON, CLR_BLACK),
/* With shuffled appearances... */
    HELM("N{helmet}", "A{plumed}",
         0, 0, 0, 10, 1, 30, 10, 9, 0, IRON, HI_METAL),
    HELM("N{q,N{helm},N{brilliance}}", "A{etched}",
         0, 1, 0, 6, 1, 50, 50, 9, 0, IRON, CLR_GREEN),
    HELM("N{q,N{helm},N{N{alignment},A{opposite}}}", "A{crested}",
         0, 1, 0, 6, 1, 50, 50, 9, 0, IRON, HI_METAL),
    HELM("N{q,N{helm},N{telepathy}}", "A{visored}",
         0, 1, TELEPAT, 2, 1, 50, 50, 9, 0, IRON, HI_METAL),

/* suits of armor */
/*
 * There is code in polyself.c that assumes (1) and (2).
 * There is code in obj.h, objnam.c, mon.c, read.c that assumes (2).
 *
 *	(1) The dragon scale mails and the dragon scales are together.
 *	(2) That the order of the dragon scale mail and dragon scales is the
 *	    the same defined in monst.c.
 */
#define DRGN_ARMR(name,mgc,power,cost,ac,color) \
	ARMOR(name,NULL,1,mgc,1,power,0,5,40,cost,ac,0,ARM_SUIT,DRAGON_HIDE,color)
#define DSM(color) "N{N{N{scale mail},A{N{dragon}}},A{" #color "}}"
#define DSMs(color) "N{*,N{N{N{scale},A{N{dragon}}},A{" #color "}}}"

/* 3.4.1: dragon scale mail reclassified as 'magic' since magic is
   needed to create them */
    DRGN_ARMR(DSM(gray), 1, ANTIMAGIC, 1200, 1, CLR_GRAY),
    DRGN_ARMR(DSM(silver), 1, REFLECTING, 1200, 1, DRAGON_SILVER),
    DRGN_ARMR(DSM(red), 1, FIRE_RES, 900, 1, CLR_RED),
    DRGN_ARMR(DSM(white), 1, COLD_RES, 900, 1, CLR_WHITE),
    DRGN_ARMR(DSM(orange), 1, SLEEP_RES, 900, 1, CLR_ORANGE),
    DRGN_ARMR(DSM(black), 1, DISINT_RES, 1200, 1, CLR_BLACK),
    DRGN_ARMR(DSM(blue), 1, SHOCK_RES, 900, 1, CLR_BLUE),
    DRGN_ARMR(DSM(green), 1, POISON_RES, 900, 1, CLR_GREEN),
    DRGN_ARMR(DSM(yellow), 1, ACID_RES, 900, 1, CLR_YELLOW),

/* For now, only dragons leave these. */
/* 3.4.1: dragon scales left classified as 'non-magic'; they confer
   magical properties but are produced 'naturally' */
    DRGN_ARMR(DSMs(gray), 0, ANTIMAGIC, 700, 7, CLR_GRAY),
    DRGN_ARMR(DSMs(silver), 0, REFLECTING, 700, 7, DRAGON_SILVER),
    DRGN_ARMR(DSMs(red), 0, FIRE_RES, 500, 7, CLR_RED),
    DRGN_ARMR(DSMs(white), 0, COLD_RES, 500, 7, CLR_WHITE),
    DRGN_ARMR(DSMs(orange), 0, SLEEP_RES, 500, 7, CLR_ORANGE),
    DRGN_ARMR(DSMs(black), 0, DISINT_RES, 700, 7, CLR_BLACK),
    DRGN_ARMR(DSMs(blue), 0, SHOCK_RES, 500, 7, CLR_BLUE),
    DRGN_ARMR(DSMs(green), 0, POISON_RES, 500, 7, CLR_GREEN),
    DRGN_ARMR(DSMs(yellow), 0, ACID_RES, 500, 7, CLR_YELLOW),
#undef DRGN_ARMR
#undef DSM
#undef DSMs

    ARMOR("N{plate mail}", NULL,
          1, 0, 1, 0, 44, 5, 450, 600, 3, 2, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{N{plate mail},A{crystal}}", NULL,
          1, 0, 1, 0, 10, 5, 450, 820, 3, 2, ARM_SUIT, GLASS, CLR_WHITE),
    ARMOR("N{N{plate mail},A{bronze}}", NULL,
          1, 0, 1, 0, 25, 5, 450, 400, 4, 0, ARM_SUIT, COPPER, HI_COPPER),
    ARMOR("N{splint mail}", NULL,
          1, 0, 1, 0, 62, 5, 400, 80, 4, 1, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{banded mail}", NULL,
          1, 0, 1, 0, 72, 5, 350, 90, 4, 0, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{N{mithril-coat},A{dwarvish}}", NULL,
          1, 0, 0, 0, 10, 1, 150, 240, 4, 3, ARM_SUIT, MITHRIL, HI_METAL),
    ARMOR("N{N{mithril-coat},A{elven}}", NULL,
          1, 0, 0, 0, 15, 1, 150, 240, 5, 3, ARM_SUIT, MITHRIL, HI_METAL),
    ARMOR("N{chain mail}", NULL,
          1, 0, 0, 0, 72, 5, 300, 75, 5, 1, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{N{chain mail},A{orcish}}", NULL,
          0, 0, 0, 0, 20, 5, 300, 75, 6, 1, ARM_SUIT, IRON, CLR_BLACK),
    ARMOR("N{scale mail}", NULL,
          1, 0, 0, 0, 72, 5, 250, 45, 6, 0, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{N{N{armor},A{leather}},A{studded}}", NULL,
          1, 0, 0, 0, 72, 3, 200, 15, 7, 1, ARM_SUIT, LEATHER, HI_LEATHER),
    ARMOR("N{ring mail}", NULL,
          1, 0, 0, 0, 72, 5, 250, 100, 7, 0, ARM_SUIT, IRON, HI_METAL),
    ARMOR("N{N{ring mail},A{orcish}}", NULL,
          0, 0, 0, 0, 20, 5, 250, 80, 8, 1, ARM_SUIT, IRON, CLR_BLACK),
    ARMOR("N{N{armor},A{leather}}", NULL,
          1, 0, 0, 0, 82, 3, 150, 5, 8, 0, ARM_SUIT, LEATHER, HI_LEATHER),
    ARMOR("N{N{jacket},A{leather}}", NULL,
          1, 0, 0, 0, 12, 0, 30, 10, 9, 0, ARM_SUIT, LEATHER, CLR_BLACK),

/* shirts */
    ARMOR("N{N{shirt},A{Hawaiian}}", NULL,
          1, 0, 0, 0, 8, 0, 5, 3, 10, 0, ARM_SHIRT, CLOTH, CLR_MAGENTA),
    ARMOR("N{T-shirt}", NULL,
          1, 0, 0, 0, 2, 0, 5, 2, 10, 0, ARM_SHIRT, CLOTH, CLR_WHITE),

/* cloaks */
    CLOAK("N{N{wrapping},A{N{mummy}}}", NULL,
          1, 0, 0, 0, 0, 3, 2, 10, 1, CLOTH, CLR_GRAY),
    CLOAK("N{N{cloak},A{elven}}", "A{faded}",
          0, 1, STEALTH, 8, 0, 10, 60, 9, 3, CLOTH, CLR_BLACK),
    CLOAK("N{N{cloak},A{orcish}}", "A{coarse}",
          0, 0, 0, 8, 0, 10, 40, 10, 2, CLOTH, CLR_BLACK),
    CLOAK("N{N{cloak},A{dwarvish}}", "A{hooded}",
          0, 0, 0, 8, 0, 10, 50, 10, 2, CLOTH, HI_CLOTH),
    CLOAK("N{N{cloak},A{oilskin}}", "A{slippery}",
          0, 0, 0, 8, 0, 10, 50, 9, 3, CLOTH, HI_CLOTH),
    CLOAK("N{robe}", NULL,
          1, 1, 0, 3, 0, 15, 50, 8, 3, CLOTH, CLR_RED),
    CLOAK("N{N{smock},A{N{alchemy}}}", NULL,
          0, 1, POISON_RES, 9, 0, 10, 50, 9, 1, CLOTH, CLR_WHITE),
    CLOAK("N{N{cloak},A{leather}}", NULL,
          1, 0, 0, 8, 0, 15, 40, 9, 1, LEATHER, CLR_BROWN),
/* With shuffled appearances... */
    CLOAK("N{q,N{cloak},N{protection}}", "A{tattered}",
          0, 1, PROTECTION, 9, 0, 10, 50, 7, 3, CLOTH, HI_CLOTH),
    CLOAK("N{q,N{cloak},N{invisibility}}", "A{opera}",
          0, 1, INVIS, 10, 0, 10, 60, 9, 2, CLOTH, CLR_BRIGHT_MAGENTA),
    CLOAK("N{q,N{cloak},N{N{resistance},A{N{magic}}}}", "A{ornamental}",
          0, 1, ANTIMAGIC, 2, 0, 10, 60, 9, 3, CLOTH, CLR_WHITE),
    CLOAK("N{q,N{cloak},N{displacement}}", "A{piecemeal}",
          0, 1, DISPLACED, 10, 0, 10, 50, 9, 2, CLOTH, HI_CLOTH),

/* shields */
    SHIELD("N{N{shield},A{small}}", NULL,
           1, 0, 0, 0, 6, 0, 30, 3, 9, 0, WOOD, HI_WOOD),
    SHIELD("N{N{shield},A{elven}}", "A{+,A{blue},A{green}}",
           0, 0, 0, 0, 2, 0, 40, 7, 8, 0, WOOD, CLR_GREEN),
    SHIELD("N{N{shield},A{N{Uruk-hai}}}", "A{white-handed}",
           0, 0, 0, 0, 2, 0, 50, 7, 9, 0, IRON, HI_METAL),
    SHIELD("N{N{shield},A{orcish}}", "A{red-eyed}",
           0, 0, 0, 0, 2, 0, 50, 7, 9, 0, IRON, CLR_RED),
    SHIELD("N{N{shield},A{large}}", NULL,
           1, 0, 1, 0, 7, 0, 100, 10, 8, 0, IRON, HI_METAL),
    SHIELD("N{N{roundshield},A{dwarvish}}", "A{round}",
           0, 0, 0, 0, 4, 0, 100, 10, 8, 0, IRON, HI_METAL),
    SHIELD("N{q,N{shield},N{reflection}}", "A{polished}",
           0, 1, 0, REFLECTING, 3, 0, 50, 50, 8, 0, SILVER, HI_SILVER),

/* gloves */
/* these have their color but not material shuffled, so the IRON must stay
 * CLR_BROWN (== HI_LEATHER)
 */
    GLOVES("N{N{*,N{glove}},A{leather}}", "A{old}",
           0, 0, 0, 16, 1, 10, 8, 9, 0, LEATHER, HI_LEATHER),
    GLOVES("N{q,N{*,N{gauntlet}},N{fumbling}}", "A{padded}",
           0, 1, FUMBLING, 8, 1, 10, 50, 9, 0, LEATHER, HI_LEATHER),
    GLOVES("N{q,N{*,N{gauntlet}},N{power}}", "A{riding}",
           0, 1, 0, 8, 1, 10, 50, 9, 0, IRON, CLR_BROWN),
    GLOVES("N{q,N{*,N{gauntlet}},N{dexterity}}", "A{fencing}",
           0, 1, 0, 8, 1, 10, 50, 9, 0, LEATHER, HI_LEATHER),

/* boots */
    BOOTS("N{N{*,N{boot}},A{low}}", "A{walking}",
          0, 0, 0, 25, 2, 10, 8, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{shoe}},A{iron}}", "A{hard}",
          0, 0, 0, 7, 2, 50, 16, 8, 0, IRON, HI_METAL),
    BOOTS("N{N{*,N{boot}},A{high}}", NULL,
          0, 0, 0, 15, 2, 20, 12, 8, 0, LEATHER, HI_LEATHER),
/* With shuffled appearances... */
    BOOTS("N{N{*,N{boot}},A{N{speed}}}", "A{combat}",
          0, 1, FAST, 12, 2, 20, 50, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{boot}},A{N{water walking}}}", "A{jungle}",
          0, 1, WWALKING, 12, 2, 20, 50, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{boot}},A{N{jumping}}}", "A{hiking}",
          0, 1, JUMPING, 12, 2, 20, 50, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{boot}},A{elven}}", "A{mud}",
          0, 1, STEALTH, 12, 2, 15, 8, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{boot}},A{N{kicking}}}", "A{buckled}",
          0, 1, 0, 12, 2, 15, 8, 9, 0, IRON, CLR_BROWN),
    BOOTS("N{N{*,N{boot}},A{N{fumble}}}", "A{riding}",
          0, 1, FUMBLING, 12, 2, 20, 30, 9, 0, LEATHER, HI_LEATHER),
    BOOTS("N{N{*,N{boot}},A{N{levitation}}}", "A{snow}",
          0, 1, LEVITATION, 12, 2, 15, 30, 9, 0, LEATHER, HI_LEATHER),
#undef HELM
#undef CLOAK
#undef SHIELD
#undef GLOVES
#undef BOOTS
#undef ARMOR

/* rings ... */
#define RING(name,power,stone,cost,mgc,spec,mohs,metal,color) OBJECT( \
		OBJ(name,stone), \
		BITS(0,0,spec,0,mgc,spec,0,0,0,HARDGEM(mohs),0,P_NONE,metal), \
		power, RING_CLASS, 0, 0, 3, cost, 0, 0, 0, 0, 15, color )
    RING("N{adornment}", ADORNED, "A{wooden}", 100, 1, 1, 2, WOOD, HI_WOOD),
    RING("N{gain strength}", 0, "A{granite}", 150, 1, 1, 7, MINERAL,
         HI_MINERAL),
    RING("N{gain constitution}", 0, "A{opal}", 150, 1, 1, 7, MINERAL,
         HI_MINERAL),
    RING("N{increase accuracy}", 0, "A{clay}", 150, 1, 1, 4, MINERAL, CLR_RED),
    RING("N{increase damage}", 0, "A{coral}", 150, 1, 1, 4, MINERAL,
         CLR_ORANGE),
    RING("N{protection}", PROTECTION, "A{onyx}", 100, 1, 1, 7, MINERAL,
         CLR_BLACK),
    RING("N{regeneration}", REGENERATION, "A{moonstone}",
         200, 1, 0, 6, MINERAL, HI_MINERAL),
    RING("N{searching}", SEARCHING, "A{tiger eye}", 200, 1, 0, 6, GEMSTONE,
         CLR_BROWN),
    RING("N{stealth}", STEALTH, "A{jade}", 100, 1, 0, 6, GEMSTONE, CLR_GREEN),
    RING("N{sustain ability}", FIXED_ABIL, "A{bronze}",
         100, 1, 0, 4, COPPER, HI_COPPER),
    RING("N{levitation}", LEVITATION, "A{agate}", 200, 1, 0, 7, GEMSTONE,
         CLR_RED),
    RING("N{hunger}", HUNGER, "A{topaz}", 100, 1, 0, 8, GEMSTONE, CLR_CYAN),
    RING("N{aggravate monster}", AGGRAVATE_MONSTER, "A{sapphire}",
         150, 1, 0, 9, GEMSTONE, CLR_BLUE),
    RING("N{conflict}", CONFLICT, "A{ruby}", 300, 1, 0, 9, GEMSTONE, CLR_RED),
    RING("N{warning}", WARNING, "A{diamond}", 100, 1, 0, 10, GEMSTONE,
         CLR_WHITE),
    RING("N{N{resistance},A{N{poison}}}", POISON_RES, "A{pearl}",
         150, 1, 0, 4, BONE, CLR_WHITE),
    RING("N{N{resistance},A{N{fire}}}", FIRE_RES, "A{iron}", 200, 1, 0, 5, IRON,
         HI_METAL),
    RING("N{N{resistance},A{N{cold}}}", COLD_RES, "A{brass}", 150, 1, 0, 4, COPPER,
         HI_COPPER),
    RING("N{N{resistance},A{N{shock}}}", SHOCK_RES, "A{copper}",
         150, 1, 0, 3, COPPER, HI_COPPER),
    RING("N{free action}", FREE_ACTION, "A{twisted}",
         200, 1, 0, 6, IRON, HI_METAL),
    RING("N{slow digestion}", SLOW_DIGESTION, "A{steel}",
         200, 1, 0, 8, IRON, HI_METAL),
    RING("N{teleportation}", TELEPORT, "A{silver}", 200, 1, 0, 3, SILVER,
         HI_SILVER),
    RING("N{N{control},A{N{teleport}}}", TELEPORT_CONTROL, "A{gold}",
         300, 1, 0, 3, GOLD, HI_GOLD),
    RING("N{polymorph}", POLYMORPH, "A{ivory}", 300, 1, 0, 4, BONE, CLR_WHITE),
    RING("N{N{control},A{N{polymorph}}}", POLYMORPH_CONTROL, "A{emerald}",
         300, 1, 0, 8, GEMSTONE, CLR_BRIGHT_GREEN),
    RING("N{invisibility}", INVIS, "A{wire}", 150, 1, 0, 5, IRON, HI_METAL),
    RING("N{see invisible}", SEE_INVIS, "A{engagement}",
         150, 1, 0, 5, IRON, HI_METAL),
    RING("N{protection from shape changers}", PROT_FROM_SHAPE_CHANGERS,
         "A{shiny}", 100, 1, 0, 5, IRON, CLR_BRIGHT_CYAN),
#undef RING

/* amulets ... - THE Amulet comes last because it is special */
#define AMULET(name,desc,power,prob) OBJECT( \
		OBJ(name,desc), BITS(0,0,0,0,1,0,0,0,0,0,0,P_NONE,IRON), power, \
		AMULET_CLASS, prob, 0, 20, 150, 0, 0, 0, 0, 20, HI_METAL )

    AMULET("N{q,N{amulet},N{ESP}}", "A{circular}", TELEPAT, 175),
    AMULET("N{q,N{amulet},N{life saving}}", "A{spherical}", LIFESAVED, 75),
    AMULET("N{q,N{amulet},N{strangulation}}", "A{oval}", STRANGLED, 135),
    AMULET("N{q,N{amulet},N{restful sleep}}", "A{triangular}", SLEEPING, 135),
    AMULET("N{amulet versus poison}", "A{pyramidal}", POISON_RES, 165),
    AMULET("N{q,N{amulet},N{change}}", "A{square}", 0, 130),
    /* POLYMORPH */
    AMULET("N{q,N{amulet},N{unchanging}}", "A{concave}", UNCHANGING, 45),
    AMULET("N{q,N{amulet},N{reflection}}", "A{hexagonal}", REFLECTING, 75),
    AMULET("N{q,N{amulet},N{magical breathing}}", "A{octagonal}",
           MAGICAL_BREATHING, 65),
    OBJECT(OBJ("N{N{N{l,N{imitation},N{Amulet of Yendor}},"
               "A{plastic}},A{cheap}}", "A{important-looking}"),
           BITS(0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, PLASTIC), 0,
           AMULET_CLASS, 0, 0, 20, 0, 0, 0, 0, 0, 1, HI_METAL),
    OBJECT(OBJ("N{Amulet of Yendor}", "A{important-looking}"),
           BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, MITHRIL), 0,
           AMULET_CLASS, 0, 0, 20, 30000, 0, 0, 0, 0, 20, HI_METAL),
#undef AMULET

/* tools ... */
/* tools with weapon characteristics come last */
#define TOOL(name,desc,kn,mrg,mgc,chg,prob,wt,cost,mat,color) \
	OBJECT( OBJ(name,desc), \
		BITS(kn,mrg,chg,0,mgc,chg,0,0,0,0,0,P_NONE,mat), \
		0, TOOL_CLASS, prob, 0, \
		wt, cost, 0, 0, 0, 0, wt, color )
#define LIGHTSOURCE(name,desc,kn,mrg,mgc,chg,prob,wt,cost,mat,color) \
       OBJECT( OBJ(name,desc), \
               BITS(kn,mrg,1,0,mgc,chg,0,0,0,0,0,P_NONE,mat), \
               0, TOOL_CLASS, prob, 0, \
               wt, cost, 0, 0, 0, 0, wt, color )
#define CONTAINER(name,desc,kn,mgc,chg,prob,wt,cost,mat,color) \
	OBJECT( OBJ(name,desc), \
		BITS(kn,0,chg,1,mgc,chg,0,0,0,0,0,P_NONE,mat), \
		0, TOOL_CLASS, prob, 0, \
		wt, cost, 0, 0, 0, 0, wt, color )
#define WEPTOOL(name,desc,kn,mgc,bi,prob,wt,cost,sdam,ldam,hitbon,sub,mat,clr) \
	OBJECT( OBJ(name,desc), \
		BITS(kn,0,1,0,mgc,1,0,0,bi,0,hitbon,sub,mat), \
		0, TOOL_CLASS, prob, 0, \
		wt, cost, sdam, ldam, hitbon, 0, wt, clr )
/* containers */
    CONTAINER("N{N{box},A{large}}", NULL, 1, 0, 0, 40, 350, 8, WOOD, HI_WOOD),
    CONTAINER("N{chest}", NULL, 1, 0, 0, 35, 600, 16, WOOD, HI_WOOD),
    CONTAINER("N{N{box},A{N{ice}}}", NULL, 1, 0, 0, 5, 900, 42, PLASTIC,
              CLR_WHITE),
    CONTAINER("N{sack}", UNID_ADJ, 0, 0, 0, 35, 15, 2, CLOTH, HI_CLOTH),
    CONTAINER("N{N{sack},A{oilskin}}", UNID_ADJ, 0, 0, 0, 5, 15, 100, CLOTH,
              HI_CLOTH),
    CONTAINER("N{q,N{bag},N{holding}}", UNID_ADJ, 0, 1, 0, 20, 15, 100, CLOTH,
              HI_CLOTH),
    CONTAINER("N{q,N{bag},N{*,N{trick}}}",UNID_ADJ, 0, 1, 1, 20, 15, 100,
              CLOTH, HI_CLOTH),
#undef CONTAINER

/* lock opening tools */
    TOOL("N{skeleton key}", NULL, 0, 0, 0, 0, 80, 3, 10, IRON, HI_METAL),
    TOOL("N{lock pick}", NULL, 1, 0, 0, 0, 60, 4, 20, IRON, HI_METAL),
    TOOL("N{credit card}", NULL, 1, 0, 0, 0, 15, 1, 10, PLASTIC, CLR_WHITE),
/* light sources */
    LIGHTSOURCE("N{N{candle},A{tallow}}", UNID_ADJ, 0, 1, 0, 0, 20, 2, 10, WAX,
                CLR_WHITE),
    LIGHTSOURCE("N{N{candle},A{wax}}", UNID_ADJ, 0, 1, 0, 0, 5, 2, 20, WAX,
                CLR_WHITE),
    LIGHTSOURCE("N{N{lantern},A{brass}}", NULL, 1, 0, 0, 0, 30, 30, 12, COPPER,
                CLR_YELLOW),
    LIGHTSOURCE("N{N{lamp},A{oil}}", UNID_ADJ, 0, 0, 0, 0, 45, 20, 10, COPPER,
                CLR_YELLOW),
/* magic lamps can't run out, so are marked as TOOL instead */
    TOOL("N{N{lamp},A{magic}}", UNID_ADJ, 0, 0, 1, 0, 15, 20, 50, COPPER,
         CLR_YELLOW),
/* other tools */
    TOOL("N{N{camera},A{expensive}}", NULL,
         1, 0, 0, 1, 15, 12, 200, PLASTIC, CLR_BLACK),
    TOOL("N{mirror}", NULL, 0, 0, 0, 0, 45, 13, 10, GLASS, HI_SILVER),
    TOOL("N{crystal ball}", NULL, 0, 0, 1, 1, 15, 150, 60, GLASS, HI_GLASS),
    TOOL("N{lenses}", NULL, 1, 0, 0, 0, 5, 3, 80, GLASS, HI_GLASS),
    TOOL("N{blindfold}", NULL, 1, 0, 0, 0, 50, 2, 20, CLOTH, CLR_BLACK),
    TOOL("N{towel}", NULL, 1, 0, 0, 0, 50, 2, 50, CLOTH, CLR_MAGENTA),
    TOOL("N{saddle}", NULL, 1, 0, 0, 0, 5, 200, 150, LEATHER, HI_LEATHER),
    TOOL("N{leash}", NULL, 1, 0, 0, 0, 65, 12, 20, LEATHER, HI_LEATHER),
    TOOL("N{stethoscope}", NULL, 1, 0, 0, 0, 25, 4, 75, IRON, HI_METAL),
    TOOL("N{tinning kit}", NULL, 1, 0, 0, 1, 15, 100, 30, IRON, HI_METAL),
    TOOL("N{tin opener}", NULL, 1, 0, 0, 0, 35, 4, 30, IRON, HI_METAL),
    TOOL("N{f,N{grease},N{can}}", NULL, 1, 0, 0, 1, 15, 15, 20, IRON, HI_METAL),
    TOOL("N{figurine}", NULL, 1, 0, 1, 0, 25, 50, 80, MINERAL, HI_MINERAL),
    TOOL("N{magic marker}", NULL, 1, 0, 1, 1, 15, 2, 50, PLASTIC, CLR_RED),
/* traps */
    TOOL("N{land mine}", NULL, 1, 0, 0, 0, 0, 300, 180, IRON, CLR_RED),
    TOOL("N{beartrap}", NULL, 1, 0, 0, 0, 0, 200, 60, IRON, HI_METAL),
/* instruments */
    TOOL("N{N{whistle},A{tin}}", UNID_ADJ, 0, 0, 0, 0, 100, 3, 10, METAL,
         HI_METAL),
    TOOL("N{N{whistle},A{magic}}", UNID_ADJ, 0, 0, 1, 0, 30, 3, 10, METAL,
         HI_METAL),
/* 'If tin whistles are made out of tin, what do they make foghorns out of?' */
    TOOL("N{N{flute},A{wooden}}", UNID_ADJ, 0, 0, 0, 0, 4, 5, 12,
         WOOD, HI_WOOD),
    TOOL("N{N{flute},A{magic}}", UNID_ADJ, 0, 0, 1, 1, 2, 5, 36,
         WOOD, HI_WOOD),
    TOOL("N{N{horn},A{tooled}}", UNID_ADJ, 0, 0, 0, 0, 5, 18, 15,
         BONE, CLR_WHITE),
    TOOL("N{N{horn},A{frost}}", UNID_ADJ, 0, 0, 1, 1, 2, 18, 50,
         BONE, CLR_WHITE),
    TOOL("N{N{horn},A{fire}}", UNID_ADJ, 0, 0, 1, 1, 2, 18, 50,
         BONE, CLR_WHITE),
    TOOL("N{q,N{horn},N{plenty}}", UNID_ADJ, 0, 0, 1, 1, 2, 18, 50,
         BONE, CLR_WHITE),
    TOOL("N{N{harp},A{wooden}}", UNID_ADJ, 0, 0, 0, 0, 4, 30, 50, 
         WOOD, HI_WOOD),
    TOOL("N{N{harp},A{magic}}", UNID_ADJ, 0, 0, 1, 1, 2, 30, 50,
         WOOD, HI_WOOD),
    TOOL("N{bell}", NULL, 1, 0, 0, 0, 2, 30, 50, COPPER, HI_COPPER),
    TOOL("N{bugle}", NULL, 1, 0, 0, 0, 4, 10, 15, COPPER, HI_COPPER),
    TOOL("N{N{drum},A{leather}}", UNID_ADJ, 0, 0, 0, 0, 4, 25, 25,
         LEATHER, HI_LEATHER),
    TOOL("N{q,N{drum},N{earthquake}}", UNID_ADJ,
         0, 0, 1, 1, 2, 25, 25, LEATHER, HI_LEATHER),
/* tools useful as weapons */
    WEPTOOL("N{pick-axe}", NULL,
            1, 0, 0, 20, 100, 50, 6, 3, WHACK, P_PICK_AXE, IRON, HI_METAL),
    WEPTOOL("N{grappling hook}", NULL,
            0, 0, 0, 5, 30, 50, 2, 6, WHACK, P_FLAIL, IRON, HI_METAL),
/* 3.4.1: unicorn horn left classified as 'magic' */
    WEPTOOL("N{N{horn},A{N{unicorn}}}", NULL,
            1, 1, 1, 0, 20, 100, 12, 12, PIERCE, P_UNICORN_HORN, BONE,
            CLR_WHITE),

/* two special unique artifact 'tools' */
    OBJECT(OBJ("N{q,N{Candelabrum},N{Invocation}}", UNID_ADJ),
           BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, P_NONE, GOLD), 0,
           TOOL_CLASS, 0, 0, 10, 5000, 0, 0, 0, 0, 200, HI_GOLD),
    OBJECT(OBJ("N{q,N{Bell},N{Opening}}", "A{silver}"),
           BITS(0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, P_NONE, SILVER), 0,
           TOOL_CLASS, 0, 0, 10, 5000, 0, 0, 0, 0, 50, HI_SILVER),
#undef TOOL
#undef WEPTOOL

/* Comestibles ... */
#define FOOD(name,prob,delay,wt,unk,tin,nutrition,color) OBJECT( \
		OBJ(name,NULL), BITS(1,1,unk,0,0,0,0,0,0,0,0,P_NONE,tin), 0, \
		FOOD_CLASS, prob, delay, \
		wt, nutrition/20 + 5, 0, 0, 0, 0, nutrition, color )
/* all types of food (except tins & corpses) must have a delay of at least 1. */
/* delay on corpses is computed and is weight dependant */
/* dog eats foods 0-4 but prefers tripe rations above all others */
/* fortune cookies can be read */
/* carrots improve your vision */
/* +0 tins contain monster meat */
/* +1 tins (of spinach) make you stronger (like Popeye) */
/* food CORPSE is a cadaver of some type */
/* meatballs/sticks/rings are only created from objects via stone to flesh */

/* meat */
    FOOD("N{tripe ration}", 140, 2, 10, 0, FLESH, 200, CLR_BROWN),
    FOOD("N{corpse}", 0, 1, 0, 0, FLESH, 0, CLR_BROWN),
    FOOD("N{egg}", 85, 1, 1, 1, FLESH, 80, CLR_WHITE),
    FOOD("N{meatball}", 0, 1, 1, 0, FLESH, 5, CLR_BROWN),
    FOOD("N{meat stick}", 0, 1, 1, 0, FLESH, 5, CLR_BROWN),
    FOOD("N{f,N{meat},N{N{chunk},A{huge}}}", 0, 20, 400, 0, FLESH,
         2000, CLR_BROWN),
/* special case because it's not mergable */
    OBJECT(OBJ("N{meat ring}", NULL),
           BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, FLESH),
           0, FOOD_CLASS, 0, 1, 5, 1, 0, 0, 0, 0, 5, CLR_BROWN),

/* fruits & veggies */
    FOOD("N{kelp frond}", 0, 1, 1, 0, VEGGY, 30, CLR_GREEN),
    FOOD("N{eucalyptus leaf}", 3, 1, 1, 0, VEGGY, 30, CLR_GREEN),
    FOOD("N{apple}", 15, 1, 2, 0, VEGGY, 50, CLR_RED),
    FOOD("N{orange}", 10, 1, 2, 0, VEGGY, 80, CLR_ORANGE),
    FOOD("N{pear}", 10, 1, 2, 0, VEGGY, 50, CLR_BRIGHT_GREEN),
    FOOD("N{melon}", 10, 1, 5, 0, VEGGY, 100, CLR_BRIGHT_GREEN),
    FOOD("N{banana}", 10, 1, 2, 0, VEGGY, 80, CLR_YELLOW),
    FOOD("N{carrot}", 15, 1, 2, 0, VEGGY, 50, CLR_ORANGE),
    FOOD("N{f,N{wolfsbane},N{sprig}}", 7, 1, 1, 0, VEGGY, 40, CLR_GREEN),
    FOOD("N{f,N{garlic},N{clove}}", 7, 1, 1, 0, VEGGY, 40, CLR_WHITE),
    FOOD("N{slime mold}", 75, 1, 5, 0, VEGGY, 250, HI_ORGANIC),

/* people food */
    FOOD("N{f,N{royal jelly},N{lump}}", 0, 1, 2, 0, VEGGY, 200, CLR_YELLOW),
    FOOD("N{cream pie}", 25, 1, 10, 0, VEGGY, 100, CLR_WHITE),
    FOOD("N{candy bar}", 13, 1, 2, 0, VEGGY, 100, CLR_BROWN),
    FOOD("N{fortune cookie}", 55, 1, 1, 0, VEGGY, 40, CLR_YELLOW),
    FOOD("N{pancake}", 25, 2, 2, 0, VEGGY, 200, CLR_YELLOW),
    FOOD("N{lembas wafer}", 20, 2, 5, 0, VEGGY, 800, CLR_WHITE),
    FOOD("N{cram ration}", 20, 3, 15, 0, VEGGY, 600, HI_ORGANIC),
    FOOD("N{food ration}", 380, 5, 20, 0, VEGGY, 800, HI_ORGANIC),
    FOOD("N{K-ration}", 0, 1, 10, 0, VEGGY, 400, HI_ORGANIC),
    FOOD("N{C-ration}", 0, 1, 10, 0, VEGGY, 300, HI_ORGANIC),
    FOOD("N{tin}", 75, 0, 10, 1, METAL, 0, HI_METAL),
#undef FOOD

/* potions ... */
#define POTION(name,desc,mgc,power,prob,cost,color) OBJECT( \
		OBJ(name,desc), BITS(0,1,0,0,mgc,0,0,0,0,0,0,P_NONE,GLASS), power, \
		POTION_CLASS, prob, 0, 20, cost, 0, 0, 0, 0, 10, color )
    POTION("N{gain ability}", "A{ruby}", 1, 0, 42, 300, CLR_RED),
    POTION("N{restore ability}", "A{pink}", 1, 0, 40, 100, CLR_BRIGHT_MAGENTA),
    POTION("N{confusion}", "A{orange}", 1, CONFUSION, 42, 100, CLR_ORANGE),
    POTION("N{blindness}", "A{yellow}", 1, BLINDED, 40, 150, CLR_YELLOW),
    POTION("N{paralysis}", "A{emerald}", 1, 0, 42, 300, CLR_BRIGHT_GREEN),
    POTION("N{speed}", "A{dark green}", 1, FAST, 42, 200, CLR_GREEN),
    POTION("N{levitation}", "A{cyan}", 1, LEVITATION, 42, 200, CLR_CYAN),
    POTION("N{hallucination}", "A{sky blue}", 1, HALLUC, 40, 100, CLR_CYAN),
    POTION("N{invisibility}", "A{brilliant blue}", 1, INVIS, 40, 150,
           CLR_BRIGHT_BLUE),
    POTION("N{see invisible}", "A{magenta}", 1, SEE_INVIS, 42, 50, CLR_MAGENTA),
    POTION("N{healing}", "A{purple-red}", 1, 0, 57, 100, CLR_MAGENTA),
    POTION("N{extra healing}", "A{puce}", 1, 0, 47, 100, CLR_RED),
    POTION("N{gain level}", "A{milky}", 1, 0, 20, 300, CLR_WHITE),
    POTION("N{enlightenment}", "A{swirly}", 1, 0, 20, 200, CLR_BROWN),
    POTION("N{monster detection}", "A{bubbly}", 1, 0, 40, 150, CLR_WHITE),
    POTION("N{object detection}", "A{smoky}", 1, 0, 42, 150, CLR_GRAY),
    POTION("N{gain energy}", "A{cloudy}", 1, 0, 42, 150, CLR_WHITE),
    POTION("N{sleeping}", "A{effervescent}", 1, 0, 42, 100, CLR_GRAY),
    POTION("N{full healing}", "A{black}", 1, 0, 10, 200, CLR_BLACK),
    POTION("N{polymorph}", "A{golden}", 1, 0, 10, 200, CLR_YELLOW),
    POTION("N{booze}", "A{brown}", 0, 0, 42, 50, CLR_BROWN),
    POTION("N{sickness}", "A{fizzy}", 0, 0, 42, 50, CLR_CYAN),
    POTION("N{fruit juice}", "A{dark}", 0, 0, 42, 50, CLR_BLACK),
    POTION("N{acid}", "A{white}", 0, 0, 10, 250, CLR_WHITE),
    POTION("N{oil}", "A{murky}", 0, 0, 30, 250, CLR_BROWN),
    POTION("N{water}", "A{clear}", 0, 0, 92, 100, CLR_CYAN),
#undef POTION

/* scrolls ... */
#define SCROLL(name,text,mgc,prob,cost) OBJECT( \
		OBJ(name,text), BITS(0,1,0,0,mgc,0,0,0,0,0,0,P_NONE,PAPER), 0, \
		SCROLL_CLASS, prob, 0, 5, cost, 0, 0, 0, 0, 6, HI_PAPER )
    SCROLL("N{enchant armor}", "N{m,S{ZELGO MER}}", 1, 63, 80),
    SCROLL("N{destroy armor}", "N{m,S{JUYED AWK YACC}}", 1, 45, 100),
    SCROLL("N{confuse monster}", "N{m,S{NR 9}}", 1, 53, 100),
    SCROLL("N{scare monster}", "N{m,S{XIXAXA XOXAXA XUXAXA}}", 1, 35, 100),
    SCROLL("N{remove curse}", "N{m,S{PRATYAVAYAH}}", 1, 65, 80),
    SCROLL("N{enchant weapon}", "N{m,S{DAIYEN FOOELS}}", 1, 80, 60),
    SCROLL("N{create monster}", "N{m,S{LEP GEX VEN ZEA}}", 1, 45, 200),
    SCROLL("N{taming}", "N{m,S{PRIRUTSENIE}}", 1, 15, 200),
    SCROLL("N{genocide}", "N{m,S{ELBIB YLOH}}", 1, 15, 300),
    SCROLL("N{light}", "N{m,S{VERR YED HORRE}}", 1, 90, 50),
    SCROLL("N{teleportation}", "N{m,S{VENZAR BORGAVVE}}", 1, 55, 100),
    SCROLL("N{gold detection}", "N{m,S{THARR}}", 1, 33, 100),
    SCROLL("N{food detection}", "N{m,S{YUM YUM}}", 1, 25, 100),
    SCROLL("N{identify}", "N{m,S{KERNOD WEL}}", 1, 180, 20),
    SCROLL("N{magic mapping}", "N{m,S{ELAM EBOW}}", 1, 45, 100),
    SCROLL("N{amnesia}", "N{m,S{DUAM XNAHT}}", 1, 35, 200),
    SCROLL("N{fire}", "N{m,S{ANDOVA BEGARIN}}", 1, 30, 100),
    SCROLL("N{earth}", "N{m,S{KIRJE}}", 1, 18, 200),
    SCROLL("N{punishment}", "N{m,S{VE FORBRYDERNE}}", 1, 15, 300),
    SCROLL("N{charging}", "N{m,S{HACKEM MUCHE}}", 1, 15, 300),
    SCROLL("N{stinking cloud}", "N{m,S{VELOX NEB}}", 1, 15, 300),
    SCROLL(NULL, "N{m,S{FOOBIE BLETCH}}", 1, 0, 100),
    SCROLL(NULL, "N{m,S{TEMOV}}", 1, 0, 100),
    SCROLL(NULL, "N{m,S{GARVEN DEH}}", 1, 0, 100),
    SCROLL(NULL, "N{m,S{READ ME}}", 1, 0, 100),
    /* these must come last because they have special descriptions */
    SCROLL("N{N{paper},A{blank}}", "A{unlabeled}", 0, 28, 60),
#undef SCROLL

/* spellbooks ... */
#define SPELL(name,desc,sub,prob,delay,level,mgc,dir,color) OBJECT( \
		OBJ(name,desc), BITS(0,0,0,0,mgc,0,0,0,0,0,dir,sub,PAPER), 0, \
		SPBOOK_CLASS, prob, delay, \
		50, level*100, 0, 0, 0, level, 20, color )
    SPELL("N{dig}", "A{parchment}", P_MATTER_SPELL, 20, 6, 5, 1, RAY, HI_PAPER),
    SPELL("N{magic missile}", "A{vellum}", P_ATTACK_SPELL, 45, 2, 2, 1, RAY,
          HI_PAPER),
    SPELL("N{fireball}", "A{ragged}", P_ATTACK_SPELL, 20, 4, 4, 1, RAY, HI_PAPER),
    SPELL("N{cone of cold}", "A{dog eared}", P_ATTACK_SPELL, 10, 7, 4, 1, RAY,
          HI_PAPER),
    SPELL("N{sleep}", "A{mottled}", P_ENCHANTMENT_SPELL, 50, 1, 1, 1, RAY, HI_PAPER),
    SPELL("N{finger of death}", "A{stained}", P_ATTACK_SPELL, 5, 10, 7, 1, RAY,
          HI_PAPER),
    SPELL("N{light}", "A{cloth}", P_DIVINATION_SPELL, 45, 1, 1, 1, NODIR, HI_CLOTH),
    SPELL("N{detect monsters}", "A{leather-bound}", P_DIVINATION_SPELL, 43, 1, 1, 1,
          NODIR, HI_LEATHER),
    SPELL("N{healing}", "A{white}", P_HEALING_SPELL, 40, 2, 1, 1, IMMEDIATE,
          CLR_WHITE),
    SPELL("N{knock}", "A{pink}", P_MATTER_SPELL, 35, 1, 1, 1, IMMEDIATE,
          CLR_BRIGHT_MAGENTA),
    SPELL("N{force bolt}", "A{red}", P_ATTACK_SPELL, 35, 2, 1, 1, IMMEDIATE, CLR_RED),
    SPELL("N{confuse monster}", "A{orange}", P_ENCHANTMENT_SPELL, 30, 2, 2, 1,
          IMMEDIATE, CLR_ORANGE),
    SPELL("N{cure blindness}", "A{yellow}", P_HEALING_SPELL, 25, 2, 2, 1, IMMEDIATE,
          CLR_YELLOW),
    SPELL("N{drain life}", "A{velvet}", P_ATTACK_SPELL, 10, 2, 2, 1, IMMEDIATE,
          CLR_MAGENTA),
    SPELL("N{slow monster}", "A{light green}", P_ENCHANTMENT_SPELL, 30, 2, 2, 1,
          IMMEDIATE, CLR_BRIGHT_GREEN),
    SPELL("N{wizard lock}", "A{dark green}", P_MATTER_SPELL, 30, 3, 2, 1, IMMEDIATE,
          CLR_GREEN),
    SPELL("N{create monster}", "A{turquoise}", P_CLERIC_SPELL, 35, 3, 2, 1, NODIR,
          CLR_BRIGHT_CYAN),
    SPELL("N{detect food}", "A{cyan}", P_DIVINATION_SPELL, 30, 3, 2, 1, NODIR,
          CLR_CYAN),
    SPELL("N{cause fear}", "A{light blue}", P_ENCHANTMENT_SPELL, 25, 3, 3, 1, NODIR,
          CLR_BRIGHT_BLUE),
    SPELL("N{clairvoyance}", "A{dark blue}", P_DIVINATION_SPELL, 15, 3, 3, 1, NODIR,
          CLR_BLUE),
    SPELL("N{cure sickness}", "A{indigo}", P_HEALING_SPELL, 32, 3, 3, 1, NODIR,
          CLR_BLUE),
    SPELL("N{charm monster}", "A{magenta}", P_ENCHANTMENT_SPELL, 20, 3, 3, 1,
          IMMEDIATE, CLR_MAGENTA),
    SPELL("N{haste self}", "A{purple}", P_ESCAPE_SPELL, 33, 4, 3, 1, NODIR,
          CLR_MAGENTA),
    SPELL("N{detect unseen}", "A{violet}", P_DIVINATION_SPELL, 20, 4, 3, 1, NODIR,
          CLR_MAGENTA),
    SPELL("N{levitation}", "A{tan}", P_ESCAPE_SPELL, 20, 4, 4, 1, NODIR, CLR_BROWN),
    SPELL("N{extra healing}", "A{plaid}", P_HEALING_SPELL, 27, 5, 3, 1, IMMEDIATE,
          CLR_GREEN),
    SPELL("N{restore ability}", "A{light brown}", P_HEALING_SPELL, 25, 5, 4, 1, NODIR,
          CLR_BROWN),
    SPELL("N{invisibility}", "A{dark brown}", P_ESCAPE_SPELL, 25, 5, 4, 1, NODIR,
          CLR_BROWN),
    SPELL("N{detect treasure}", "A{gray}", P_DIVINATION_SPELL, 20, 5, 4, 1, NODIR,
          CLR_GRAY),
    SPELL("N{remove curse}", "A{wrinkled}", P_CLERIC_SPELL, 25, 5, 3, 1, NODIR,
          HI_PAPER),
    SPELL("N{magic mapping}", "A{dusty}", P_DIVINATION_SPELL, 18, 7, 5, 1, NODIR,
          HI_PAPER),
    SPELL("N{identify}", "A{bronze}", P_DIVINATION_SPELL, 20, 6, 3, 1, NODIR,
          HI_COPPER),
    SPELL("N{turn undead}", "A{copper}", P_CLERIC_SPELL, 16, 8, 6, 1, IMMEDIATE,
          HI_COPPER),
    SPELL("N{polymorph}", "A{silver}", P_MATTER_SPELL, 10, 8, 6, 1, IMMEDIATE,
          HI_SILVER),
    SPELL("N{teleport away}", "A{gold}", P_ESCAPE_SPELL, 15, 6, 6, 1, IMMEDIATE,
          HI_GOLD),
    SPELL("N{create familiar}", "A{glittering}", P_CLERIC_SPELL, 10, 7, 6, 1, NODIR,
          CLR_WHITE),
    SPELL("N{cancellation}", "A{shining}", P_MATTER_SPELL, 15, 8, 7, 1, IMMEDIATE,
          CLR_WHITE),
    SPELL("N{protection}", "A{dull}", P_CLERIC_SPELL, 18, 3, 1, 1, NODIR, HI_PAPER),
    SPELL("N{jumping}", "A{thin}", P_ESCAPE_SPELL, 20, 3, 1, 1, IMMEDIATE, HI_PAPER),
    SPELL("N{stone to flesh}", "A{thick}", P_HEALING_SPELL, 15, 1, 3, 1, IMMEDIATE,
          HI_PAPER),
/* blank spellbook must come last because it retains its description */
    SPELL("N{N{paper},A{blank}}", "A{plain}", P_NONE, 18, 0, 0, 0, 0, HI_PAPER),
/* a special, one of a kind, spellbook */
    OBJECT(OBJ("N{Book of the Dead}", "A{papyrus}"),
           BITS(0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, P_NONE, PAPER), 0,
           SPBOOK_CLASS, 0, 0, 20, 10000, 0, 0, 0, 7, 20, HI_PAPER),
#undef SPELL

/* wands ... */
#define WAND(name,typ,prob,cost,mgc,dir,metal,color) OBJECT( \
		OBJ(name,typ), BITS(0,0,1,0,mgc,1,0,0,0,0,dir,P_NONE,metal), 0, \
		WAND_CLASS, prob, 0, 7, cost, 0, 0, 0, 0, 30, color )
    WAND("N{light}", "A{glass}", 95, 100, 1, NODIR, GLASS, HI_GLASS),
    WAND("N{secret door detection}", "A{balsa}",
         50, 150, 1, NODIR, WOOD, HI_WOOD),
    WAND("N{enlightenment}", "A{crystal}", 15, 150, 1, NODIR, GLASS, HI_GLASS),
    WAND("N{create monster}", "A{maple}", 45, 200, 1, NODIR, WOOD, HI_WOOD),
    WAND("N{wishing}", "A{pine}", 5, 500, 1, NODIR, WOOD, HI_WOOD),
    WAND("N{nothing}", "A{oak}", 25, 100, 0, IMMEDIATE, WOOD, HI_WOOD),
    WAND("N{striking}", "A{ebony}", 75, 150, 1, IMMEDIATE, WOOD, HI_WOOD),
    WAND("N{make invisible}", "A{marble}", 45, 150, 1, IMMEDIATE, MINERAL,
         HI_MINERAL),
    WAND("N{slow monster}", "A{tin}", 50, 150, 1, IMMEDIATE, METAL, HI_METAL),
    WAND("N{speed monster}", "A{brass}", 50, 150, 1, IMMEDIATE, COPPER, HI_COPPER),
    WAND("N{undead turning}", "A{copper}", 50, 150, 1, IMMEDIATE, COPPER, HI_COPPER),
    WAND("N{polymorph}", "A{silver}", 45, 200, 1, IMMEDIATE, SILVER, HI_SILVER),
    WAND("N{cancellation}", "A{platinum}", 45, 200, 1, IMMEDIATE, PLATINUM,
         CLR_WHITE),
    WAND("N{teleportation}", "A{iridium}", 45, 200, 1, IMMEDIATE, METAL,
         CLR_BRIGHT_CYAN),
    WAND("N{opening}", "A{zinc}", 25, 150, 1, IMMEDIATE, METAL, HI_METAL),
    WAND("N{locking}", "A{aluminum}", 25, 150, 1, IMMEDIATE, METAL, HI_METAL),
    WAND("N{probing}", "A{uranium}", 30, 150, 1, IMMEDIATE, METAL, HI_METAL),
    WAND("N{digging}", "A{iron}", 55, 150, 1, RAY, IRON, HI_METAL),
    WAND("N{magic missile}", "A{steel}", 50, 150, 1, RAY, IRON, HI_METAL),
    WAND("N{fire}", "A{hexagonal}", 40, 175, 1, RAY, IRON, HI_METAL),
    WAND("N{cold}", "A{short}", 40, 175, 1, RAY, IRON, HI_METAL),
    WAND("N{sleep}", "A{runed}", 50, 175, 1, RAY, IRON, HI_METAL),
    WAND("N{death}", "A{long}", 5, 500, 1, RAY, IRON, HI_METAL),
    WAND("N{lightning}", "A{curved}", 40, 175, 1, RAY, IRON, HI_METAL),
    WAND(NULL, "A{forked}", 0, 150, 1, 0, WOOD, HI_WOOD),
    WAND(NULL, "A{spiked}", 0, 150, 1, 0, IRON, HI_METAL),
    WAND(NULL, "A{jeweled}", 0, 150, 1, 0, IRON, HI_MINERAL),
#undef WAND

/* coins ... - so far, gold is all there is */
#define COIN(name,prob,metal,worth) OBJECT( \
		OBJ(name,NULL), BITS(0,1,0,0,0,0,0,0,0,0,0,P_NONE,metal), 0, \
		COIN_CLASS, prob, 0, 1, worth, 0, 0, 0, 0, 0, HI_GOLD )
    COIN("N{gold piece}", 1000, GOLD, 1),
#undef COIN

/* gems ... - includes stones and rocks but not boulders */
#define GEM(name,desc,prob,wt,gval,nutr,mohs,glass,color) OBJECT( \
	    OBJ(name,desc), \
	    BITS(0,1,0,0,0,0,0,0,0,HARDGEM(mohs),0,-P_SLING,glass), 0, \
	    GEM_CLASS, prob, 0, 1, gval, 3, 3, 0, 0, nutr, color )
#define ROCK(name,desc,kn,prob,wt,gval,sdam,ldam,mgc,nutr,mohs,glass,color) OBJECT( \
	    OBJ(name,desc), \
	    BITS(kn,1,0,0,mgc,0,0,0,0,HARDGEM(mohs),0,-P_SLING,glass), 0, \
	    GEM_CLASS, prob, 0, wt, gval, sdam, ldam, 0, 0, nutr, color )
    GEM("N{dilithium crystal}", "A{white}", 2, 1, 4500, 15, 5, GEMSTONE, CLR_WHITE),
    GEM("N{diamond}", "A{white}", 3, 1, 4000, 15, 10, GEMSTONE, CLR_WHITE),
    GEM("N{ruby}", "A{red}", 4, 1, 3500, 15, 9, GEMSTONE, CLR_RED),
    GEM("N{jacinth}", "A{orange}", 3, 1, 3250, 15, 9, GEMSTONE, CLR_ORANGE),
    GEM("N{sapphire}", "A{blue}", 4, 1, 3000, 15, 9, GEMSTONE, CLR_BLUE),
    GEM("N{black opal}", "A{black}", 3, 1, 2500, 15, 8, GEMSTONE, CLR_BLACK),
    GEM("N{emerald}", "A{green}", 5, 1, 2500, 15, 8, GEMSTONE, CLR_GREEN),
    GEM("N{turquoise}", "A{green}", 6, 1, 2000, 15, 6, GEMSTONE, CLR_GREEN),
    GEM("N{citrine}", "A{yellow}", 4, 1, 1500, 15, 6, GEMSTONE, CLR_YELLOW),
    GEM("N{aquamarine}", "A{green}", 6, 1, 1500, 15, 8, GEMSTONE, CLR_GREEN),
    GEM("N{amber}", "A{yellowish-brown}", 8, 1, 1000, 15, 2, GEMSTONE, CLR_BROWN),
    GEM("N{topaz}", "A{yellowish-brown}", 10, 1, 900, 15, 8, GEMSTONE, CLR_BROWN),
    GEM("N{jet}", "A{black}", 6, 1, 850, 15, 7, GEMSTONE, CLR_BLACK),
    GEM("N{opal}", "A{white}", 12, 1, 800, 15, 6, GEMSTONE, CLR_WHITE),
    GEM("N{chrysoberyl}", "A{yellow}", 8, 1, 700, 15, 5, GEMSTONE, CLR_YELLOW),
    GEM("N{garnet}", "A{red}", 12, 1, 700, 15, 7, GEMSTONE, CLR_RED),
    GEM("N{amethyst}", "A{violet}", 14, 1, 600, 15, 7, GEMSTONE, CLR_MAGENTA),
    GEM("N{jasper}", "A{red}", 15, 1, 500, 15, 7, GEMSTONE, CLR_RED),
    GEM("N{fluorite}", "A{violet}", 15, 1, 400, 15, 4, GEMSTONE, CLR_MAGENTA),
    GEM("N{obsidian}", "A{black}", 9, 1, 200, 15, 6, GEMSTONE, CLR_BLACK),
    GEM("N{agate}", "A{orange}", 12, 1, 200, 15, 6, GEMSTONE, CLR_ORANGE),
    GEM("N{jade}", "A{green}", 10, 1, 300, 15, 6, GEMSTONE, CLR_GREEN),
    GEM("N{N{f,N{N{glass},A{white}},N{piece}},A{worthless}}", "A{white}",
        77, 1, 0, 6, 5, GLASS, CLR_WHITE),
    GEM("N{N{f,N{N{glass},A{blue}},N{piece}},A{worthless}}", "A{blue}",
        77, 1, 0, 6, 5, GLASS, CLR_BLUE),
    GEM("N{N{f,N{N{glass},A{red}},N{piece}},A{worthless}}", "A{red}",
        77, 1, 0, 6, 5, GLASS, CLR_RED),
    GEM("N{N{f,N{N{glass},A{yellowish-brown}},N{piece}},A{worthless}}",
        "A{yellowish-brown}", 77, 1, 0, 6, 5, GLASS, CLR_BROWN),
    GEM("N{N{f,N{N{glass},A{orange}},N{piece}},A{worthless}}", "A{orange}",
        76, 1, 0, 6, 5, GLASS, CLR_ORANGE),
    GEM("N{N{f,N{N{glass},A{yellow}},N{piece}},A{worthless}}", "A{yellow}",
        77, 1, 0, 6, 5, GLASS, CLR_YELLOW),
    GEM("N{N{f,N{N{glass},A{black}},N{piece}},A{worthless}}", "A{black}",
        76, 1, 0, 6, 5, GLASS, CLR_BLACK),
    GEM("N{N{f,N{N{glass},A{green}},N{piece}},A{worthless}}", "A{green}",
        77, 1, 0, 6, 5, GLASS, CLR_GREEN),
    GEM("N{N{f,N{N{glass},A{violet}},N{piece}},A{worthless}}", "A{violet}",
        77, 1, 0, 6, 5, GLASS, CLR_MAGENTA),

/* Placement note: there is a wishable subrange for
 * 'gray stones' in the o_ranges[] array in objnam.c
 * that is currently everything between luckstones and flint (inclusive).
 */
    ROCK("N{luckstone}", "A{gray}", 0, 10, 10, 60, 3, 3, 1, 10, 7,
         MINERAL, CLR_GRAY),
    ROCK("N{loadstone}", "A{gray}", 0, 10, 500, 1, 3, 3, 1, 10, 6,
         MINERAL, CLR_GRAY),
    ROCK("N{touchstone}", "A{gray}", 0, 8, 10, 45, 3, 3, 1, 10, 6,
         MINERAL, CLR_GRAY),
    ROCK("N{flint}", "A{gray}", 0, 10, 10, 1, 6, 6, 0, 10, 7,
         MINERAL, CLR_GRAY),
    ROCK("N{rock}", NULL, 1, 100, 10, 0, 3, 3, 0, 10, 7,
         MINERAL, CLR_GRAY),
#undef GEM
#undef ROCK

/* miscellaneous ... */
/* Note: boulders and rocks are not normally created at random; the
 * probabilities only come into effect when you try to polymorph them.
 * Boulders weigh more than MAX_CARR_CAP; statues use corpsenm to take
 * on a specific type and may act as containers (both affect weight).
 * All these objects are always-IDed (and the item naming code won't
 * know how to give them an un-IDed name unless you tell it).
 */
    OBJECT(OBJ("N{boulder}", NULL),
           BITS(1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, P_NONE, MINERAL), 0,
           ROCK_CLASS, 100, 0, 6000, 0, 20, 20, 0, 0, 2000, HI_MINERAL),
    OBJECT(OBJ("N{statue}", NULL),
           BITS(1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, P_NONE, MINERAL), 0,
           ROCK_CLASS, 900, 0, 2500, 0, 20, 20, 0, 0, 2500, CLR_WHITE),

    OBJECT(OBJ("N{N{N{ball},A{iron}},A{heavy}}", NULL),
           BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, WHACK, P_NONE, IRON), 0,
           BALL_CLASS, 1000, 0, 480, 10, 25, 25, 0, 0, 200, HI_METAL),
    /* +d4 when 'very heavy' */
    OBJECT(OBJ("N{N{chain},A{iron}}", NULL),
           BITS(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, WHACK, P_NONE, IRON), 0,
           CHAIN_CLASS, 1000, 0, 120, 0, 4, 4, 0, 0, 200, HI_METAL),
    /* +1 both l & s */

    OBJECT(OBJ("N{N{venom},A{blinding}}", NULL),
           BITS(0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, P_NONE, LIQUID), 0,
           VENOM_CLASS, 500, 0, 1, 0, 0, 0, 0, 0, 0, HI_ORGANIC),
    OBJECT(OBJ("N{N{venom},A{acid}}", NULL),
           BITS(0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, P_NONE, LIQUID), 0,
           VENOM_CLASS, 500, 0, 1, 0, 6, 6, 0, 0, 0, HI_ORGANIC),
    /* +d6 small or large */

/* fencepost, the deadly Array Terminator -- name [1st arg] *must* be NULL */
    OBJECT(OBJ(NULL, NULL), BITS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, P_NONE, 0), 0,
           ILLOBJ_CLASS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
};      /* objects[] */

#ifndef OBJECTS_PASS_2_

/* perform recursive compilation for second structure */
# undef OBJ
# undef OBJECT
# define OBJECTS_PASS_2_
# include "objects.c"

#else

void
init_objlist(void)
{
    objects = malloc(sizeof (const_objects));
    memcpy(objects, const_objects, sizeof (const_objects));

}

#endif /* !OBJECTS_PASS_2_ */

/*objects.c*/
