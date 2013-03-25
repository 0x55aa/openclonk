/* Golden mountain */

func Initialize()
{
	// Goal
	var goal = FindObject(Find_ID(Goal_SellGems));
	if (!goal) goal = CreateObject(Goal_SellGems);
	goal->SetTargetAmount(30);
	// Rules
	if (!ObjectCount(Find_ID(Rule_TeamAccount))) CreateObject(Rule_TeamAccount);
	if (!ObjectCount(Find_ID(Rule_BuyAtFlagpole))) CreateObject(Rule_BuyAtFlagpole);
	return true;
}

static g_was_player_init;

func InitializePlayer(int plr)
{
	// Harsh zoom range
	for (var flag in [PLRZOOM_LimitMax, PLRZOOM_Direct])
		SetPlayerZoomByViewRange(plr,500,350,flag);
	SetPlayerViewLock(plr, true);
	// First player init base
	if (!g_was_player_init)
	{
		InitBase(plr);
		g_was_player_init = true;
	}
	// Position and materials
	var i, crew;
	for (i=0; crew=GetCrew(plr,i); ++i)
	{
		crew->SetPosition(500+Random(100), 200-10);
		crew->CreateContents(Shovel);
	}
	return true;
}

private func InitBase(int owner)
{
	// Create standard base owned by player
	var y=200;
	var flag = CreateObject(Flagpole, 590,y, owner);
	var windgen = CreateObject(WindGenerator, 500,y, owner);
	var chemlab = CreateObject(ChemicalLab, 560,y, owner);
	var invlab = CreateObject(InventorsLab, 660,y, owner);
	if (invlab)
	{
		invlab->SetClrModulation(0xff804000);
	}
	var toolsw = CreateObject(ToolsWorkshop, 620,y, owner);
	if (toolsw)
	{
		toolsw->CreateContents(Wood, 5);
		toolsw->CreateContents(Metal, 2);
	}
	var lorry = CreateObject(Lorry, 690,y-2, owner);
	if (lorry)
	{
		lorry->CreateContents(GrappleBow, GetStartupPlayerCount());
		lorry->CreateContents(JarOfWinds, 2);
		lorry->CreateContents(TeleGlove, 1);
		lorry->CreateContents(Axe, 1);
		lorry->CreateContents(Hammer, 1);
		//lorry->CreateContents(DynamiteBox, 1);
		lorry->CreateContents(Dynamite, 2);
	}
	return true;
}
