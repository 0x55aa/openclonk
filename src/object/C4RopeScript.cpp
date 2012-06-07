/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2012  Armin Burgmeier
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

#include <C4Include.h>
#include <C4Rope.h>
#include <C4AulDefFunc.h>

static C4Void FnRemove(C4PropList* Rope)
{
	Game.Ropes.RemoveRope(static_cast<C4Rope*>(Rope));
	return C4Void();
}

static C4Object* FnGetFront(C4PropList* Rope)
{
	return static_cast<C4Rope*>(Rope)->GetFront()->GetObject();
}

static C4Object* FnGetBack(C4PropList* Rope)
{
	return static_cast<C4Rope*>(Rope)->GetBack()->GetObject();
}

static C4Void FnSetFront(C4PropList* Rope, C4Object* obj, Nillable<int> x, Nillable<int> y)
{
	static_cast<C4Rope*>(Rope)->SetFront(obj, x.IsNil() ? Fix0 : itofix(x), y.IsNil() ? Fix0 : itofix(y));
	return C4Void();
}

static C4Void FnSetBack(C4PropList* Rope, C4Object* obj, Nillable<int> x, Nillable<int> y)
{
	static_cast<C4Rope*>(Rope)->SetBack(obj, x.IsNil() ? Fix0 : itofix(x), y.IsNil() ? Fix0 : itofix(y));
	return C4Void();
}

static C4Void FnSetFrontAutoSegmentation(C4PropList* Rope, int max)
{
	static_cast<C4Rope*>(Rope)->SetFrontAutoSegmentation(itofix(max));
	return C4Void();
}

static C4Void FnSetBackAutoSegmentation(C4PropList* Rope, int max)
{
	static_cast<C4Rope*>(Rope)->SetBackAutoSegmentation(itofix(max));
	return C4Void();
}

static C4Void FnSetFrontFixed(C4PropList* Rope, bool fixed)
{
	static_cast<C4Rope*>(Rope)->SetFrontFixed(fixed);
	return C4Void();
}

static C4Void FnSetBackFixed(C4PropList* Rope, bool fixed)
{
	static_cast<C4Rope*>(Rope)->SetBackFixed(fixed);
	return C4Void();
}

static C4Void FnPullFront(C4PropList* Rope, int force)
{
	static_cast<C4Rope*>(Rope)->PullFront(itofix(force));
	return C4Void();
}

static C4Void FnPullBack(C4PropList* Rope, int force)
{
	static_cast<C4Rope*>(Rope)->PullBack(itofix(force));
	return C4Void();
}

C4RopeAul::C4RopeAul():
	RopeDef(NULL)
{
}

C4RopeAul::~C4RopeAul()
{
	delete RopeDef;
}

void C4RopeAul::InitFunctionMap(C4AulScriptEngine* pEngine)
{
	delete RopeDef;
	RopeDef = C4PropList::NewAnon(NULL, NULL, ::Strings.RegString("Rope"));
	RopeDef->SetName("Rope");
	pEngine->RegisterGlobalConstant("Rope", C4VPropList(RopeDef));

	Reg2List(pEngine);

	::AddFunc(this, "Remove", FnRemove);
	::AddFunc(this, "GetFront", FnGetFront);
	::AddFunc(this, "GetBack", FnGetBack);
	::AddFunc(this, "SetFront", FnSetFront);
	::AddFunc(this, "SetBack", FnSetBack);
	::AddFunc(this, "SetFrontAutoSegmentation", FnSetFrontAutoSegmentation);
	::AddFunc(this, "SetBackAutoSegmentation", FnSetBackAutoSegmentation);
	::AddFunc(this, "SetFrontFixed", FnSetFrontFixed);
	::AddFunc(this, "SetBackFixed", FnSetBackFixed);
	::AddFunc(this, "PullFront", FnPullFront);
	::AddFunc(this, "PullBack", FnPullBack);
	RopeDef->Freeze();
}
