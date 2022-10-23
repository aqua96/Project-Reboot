#include <MinHook.h>

#include "patterns.h"
#include "server.h"
#include "loot.h"

void Server::PauseBeaconRequests(bool bPause)
{
	Defines::PauseBeaconRequests(BeaconHost, bPause);
	std::cout << "cc!\n";
	SetWorld(Helper::GetWorld());
}

void Server::SetWorld(UObject* World)
{
	if (Engine_Version < 426)
	{
		if (Defines::SetWorld)
			Defines::SetWorld(NetDriver, World);
		else
			std::cout << "Invalid SetWorld!\n";
	}
	else
	{
		if (!Defines::SetWorld)
		{
			/* int SetWorldIndex = Fortnite_Version < 19.00 ? Fortnite_Season < 15 ? 0x71
				: Fortnite_Season == 14 ? 0x72 : Fortnite_Season == 13 ? 0x70
				: Fortnite_Version >= 20.00 ? (Fortnite_Version >= 21 ? 0x7C : 0x7B) : 0x7A : ; // s13-14 = 0x71 s15-s18 = 0x72 s19 = 0x7A s20 = 7B s21 = 7c 

			*/

			int SetWorldIndex = 0;

			if (Fortnite_Season == 13)
				SetWorldIndex = 0x70;
			else if (Fortnite_Season == 14)
				SetWorldIndex = 0x71;
			else if (Fortnite_Season >= 15)
				SetWorldIndex = 0x72;

			std::cout << "SetWorldIndex: " << SetWorldIndex << '\n';

			Defines::SetWorld = decltype(Defines::SetWorld)(NetDriver->VFTable[SetWorldIndex]);
		}

		if (Defines::SetWorld)
			Defines::SetWorld(NetDriver, World);
		else
			std::cout << "Invalid SetWorld!\n";
	}
}

bool Server::Listen(int Port)
{
	if (bUseBeacons)
	{
		static UObject* BeaconClass = FindObject("/Script/FortniteGame.FortOnlineBeaconHost"); // We use the Fort one because then FindObject will not mistake for the BeaconHostObject.

		BeaconHost = Helper::Easy::SpawnActor(BeaconClass, FVector());

		if (!BeaconHost)
		{
			std::cout << "Failed to spawn Beacon!\n";
			return false;
		}

		// set listen port

		static auto ListenPortOffset = BeaconHost->GetOffset("ListenPort");
		auto ListenPort = Get<int>(BeaconHost, ListenPortOffset);

		*ListenPort = Engine_Version < 426 ? Port - 1 : Port;

		if (Defines::InitHost(BeaconHost))
		{
			std::cout << "Successfully initialized beacon!\n";

			static auto NetDriverOffset = BeaconHost->GetOffset("NetDriver");
			NetDriver = *Get<UObject*>(BeaconHost, NetDriverOffset);
		}
		else
		{
			std::cout << "Failed to init host!\n";
			return false;
		}
	}
	else
	{

	}

	if (!NetDriver)
	{
		std::cout << "Missing NetDriver!\n";
		return false;
	}

	// setup netdriver

	FString Error;
	auto InURL = FURL();
	InURL.Port = Port;

	UObject* World = Helper::GetWorld();

	static auto NetDriverNameOffset = NetDriver->GetOffset("NetDriverName");
	
	FString NetDriverNameFStr = L"GameNetDriver"; // to free
	*Get<FName>(NetDriver, NetDriverNameOffset) = Helper::Conversion::StringToName(NetDriverNameFStr);

	auto InitListenResult = Defines::InitListen(NetDriver, World, InURL, false, Error);

	// end setup

	static auto LevelCollectionStruct = FindObject("/Script/Engine.LevelCollection");
	static auto LevelCollectionsOffset = World->GetOffset("LevelCollections");
	auto LevelCollections = Get<TArray<UObject>>(World, LevelCollectionsOffset);

	if (LevelCollections && LevelCollections->Data)
	{
		static auto LevelCollectionSize = Helper::GetSizeOfClass(LevelCollectionStruct);

		std::cout << "LevelCollectionSize: " << LevelCollectionSize << '\n';

		auto FirstLevelCollection = LevelCollections->AtPtr(0, LevelCollectionSize);

		static auto LC_NetDriverOffset = 0x10; // LevelCollectionStruct->GetOffset("NetDriver", true);

		std::cout << "LC_NetDriverOffset: " << LC_NetDriverOffset << '\n';

		*Get<UObject*>(FirstLevelCollection, LC_NetDriverOffset) = NetDriver;
		*Get<UObject*>(LevelCollections->AtPtr(1, LevelCollectionSize), LC_NetDriverOffset) = NetDriver;
	}
	else
		std::cout << "Unable to find LevelCollections!\n";

	std::cout << "aa!\n";

	PauseBeaconRequests(false);

	std::cout << "bb!\n";

	if (Fortnite_Version >= 3.3)
	{
		static auto ReplicationDriverOffset = NetDriver->GetOffset("ReplicationDriver");
		auto ReplicationDriver = *Get<UObject*>(NetDriver, ReplicationDriverOffset);

		Defines::ServerReplicateActors = decltype(Defines::ServerReplicateActors)(ReplicationDriver->VFTable[ServerReplicateActorsOffset]);
	}

	std::cout << "dd!\n";

	static auto World_NetDriverOffset = World->GetOffsetSlow("NetDriver");

	if (World_NetDriverOffset != 0)
		*Get<UObject*>(World, World_NetDriverOffset) = NetDriver;

	std::cout << "Listening on port: " << Port << '\n';

	return true;
}

void Server::Hooks::Initialize()
{
	// if (false)
	{
		std::cout << MH_StatusToString(MH_CreateHook((PVOID)TickFlushAddress, Server::Hooks::TickFlush, (PVOID*)&Defines::TickFlush)) << '\n';
		std::cout << MH_StatusToString(MH_EnableHook((PVOID)TickFlushAddress)) << '\n';
	}

	std::cout << MH_StatusToString(MH_CreateHook((PVOID)KickPlayerAddress, Server::Hooks::KickPlayer, (PVOID*)&Defines::KickPlayer)) << '\n';
	std::cout << MH_StatusToString(MH_EnableHook((PVOID)KickPlayerAddress)) << '\n';

	std::cout << MH_StatusToString(MH_CreateHook((PVOID)ValidationFailureAddress, Server::Hooks::ValidationFailure, (PVOID*)&Defines::ValidationFailure)) << '\n';
	std::cout << MH_StatusToString(MH_EnableHook((PVOID)ValidationFailureAddress)) << '\n';

	std::cout << MH_StatusToString(MH_CreateHook((PVOID)NoReserveAddress, Server::Hooks::NoReservation, (PVOID*)&Defines::NoReservation)) << '\n';
	std::cout << MH_StatusToString(MH_EnableHook((PVOID)NoReserveAddress)) << '\n';

	if (Fortnite_Version < 17.00)
	{
		auto sig = Memory::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 40 48 89 11 48 8B D9 48 8B 42 30 48 85 C0 75 07 48 8B 82 ? ? ? ? 48");

		if (!sig)
			sig = Memory::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 40 48 89 11");

		std::cout << MH_StatusToString(MH_CreateHook((PVOID)sig, Server::Hooks::NetViewerConstructorDetour, nullptr)) << '\n';
		std::cout << MH_StatusToString(MH_EnableHook((PVOID)sig)) << '\n';
	}
}

void Server::Hooks::TickFlush(UObject* thisNetDriver, float DeltaSeconds)
{
	auto load = [](UObject* Class, const std::string& Name) -> UObject* {
		UObject* Object = FindObject(Name);

		if (!Object)
			Object = StaticLoadObject(Class, nullptr, Name);

		return Object;
	};

	auto World = Helper::GetWorld();

	if (World)
	{
		static auto NetDriverOffset = World->GetOffsetSlow("NetDriver");
		auto NetDriver = *Get<UObject*>(World, NetDriverOffset);

		if (NetDriver)
		{
			if (Fortnite_Version <= 3.3)
			{
				// ReplicateActors(NetDriver, World);
			}
			else
			{
				static auto ReplicationDriverOffset = NetDriver->GetOffset("ReplicationDriver");
				auto ReplicationDriver = Get<UObject*>(NetDriver, ReplicationDriverOffset);

				if (ReplicationDriver)
				{
					Defines::ServerReplicateActors(*ReplicationDriver);
				}
			}
		}
	}

	if (Defines::ObjectsToLoad.size() > 0)
	{
		for (int i = 0; i < Defines::ObjectsToLoad.size(); i++)
		{
			auto& Object = Defines::ObjectsToLoad.at(i);

			StaticLoadObject(Object.first, nullptr, Object.second);
			Defines::ObjectsToLoad.erase(Defines::ObjectsToLoad.begin() + i);
		}
	}

	if (Defines::bShouldSpawnForagedItems)
	{
		Defines::bShouldSpawnForagedItems = false;

		static auto BGAConsumableSpawnerClass = FindObject("/Script/FortniteGame.BGAConsumableSpawner");

		auto AllActors = Helper::GetAllActorsOfClass(BGAConsumableSpawnerClass);

		std::cout << "Spawning: " << AllActors.Num() << " foraged items!\n";

		for (int i = 0; i < AllActors.Num(); i++)
		{
			auto AllActor = AllActors.At(i);

			if (AllActor)
			{
				static auto SpawnLootTierGroupOffset = AllActor->GetOffset("SpawnLootTierGroup");
				auto SpawnLootTierGroupFName = Get<FName>(AllActor, SpawnLootTierGroupOffset);

				auto SpawnLootTierGroup = SpawnLootTierGroupFName->ToString();

				std::cout << "SpawnLootTierGroup: " << SpawnLootTierGroup << '\n';

				auto Location = Helper::GetActorLocation(AllActor);
				auto Rotation = Helper::GetActorRotation(AllActor);

				if (SpawnLootTierGroup == "Loot_ForagedItem_AthenaRift")
				{
					auto riftportal = load(Helper::GetBGAClass(), "/Game/Athena/Items/ForagedItems/Rift/BGA_RiftPortal_Athena.BGA_RiftPortal_Athena_C");

					Helper::Easy::SpawnActor(riftportal, Location, Rotation);
				}

				continue;

				if (SpawnLootTierGroup == "Loot_ForagedItem_SpookyMist") // Cube Consumable
				{
					auto riftportal = load(Helper::GetBGAClass(), "/Game/Athena/Items/ForagedItems/SpookyMist/CBGA_SpookyMist.CBGA_SpookyMist_C");

					Helper::Easy::SpawnActor(riftportal, Location, Rotation);
				}
			}
		}
	}

	if (Defines::bShouldSpawnVehicles)
	{
		Defines::bShouldSpawnVehicles = false;

		static auto FortVehicleSpawnerClass = FindObject("/Game/Athena/DrivableVehicles/Athena_VehicleSpawner.Athena_VehicleSpawner_C");

		auto spawnerClass = FortVehicleSpawnerClass;

		auto Spawners = Helper::GetAllActorsOfClass(spawnerClass);

		std::cout << "Spawning: " << Spawners.Num() << " vehicles\n";

		for (int i = 0; i < Spawners.Num(); i++)
		{
			auto Spawner = Spawners.At(i);

			if (!Spawner)
				continue;

			std::cout << std::format("[{}] {}\n", i, Spawner->GetFullName());

			static auto FortVehicleItemDefVariantsOffset = Spawner->GetOffset("FortVehicleItemDefVariants");

			struct FVehicleWeightedDef
			{
				TSoftObjectPtr VehicleItemDef;
				FScalableFloat                              Weight;                                                   // 0x0028(0x0020) (Edit, BlueprintVisible, BlueprintReadOnly)
			};

			auto FortVehicleItemDefVariants = Get<TArray<FVehicleWeightedDef>>(Spawner, FortVehicleItemDefVariantsOffset);

			std::cout << "FortVehicleItemDefVariants: " << FortVehicleItemDefVariants->Num() << '\n';

			static auto VIDClass = FindObject("/Script/FortniteGame.FortVehicleItemDefinition");

			if (FortVehicleItemDefVariants->Num() > 0)
			{
				auto first = FortVehicleItemDefVariants->At(0);

				auto AssetPathName = first.VehicleItemDef.ObjectID.AssetPathName;

				std::cout << "AssetPathName: " << AssetPathName.ComparisonIndex << '\n';

				if (!AssetPathName.ComparisonIndex)
					continue;

				auto VehicleItemDef = load(VIDClass, AssetPathName.ToString());

				std::cout << "VehicleItemDef: " << VehicleItemDef << '\n';

				if (VehicleItemDef)
				{
					static auto VehicleActorClassOffset = VehicleItemDef->GetOffset("VehicleActorClass");

					auto VehicleActorClassSoft = Get<TSoftObjectPtr>(VehicleItemDef, VehicleActorClassOffset);

					auto assetpathname = VehicleActorClassSoft->ObjectID.AssetPathName;

					std::cout << "assetpathname sof: " << assetpathname.ComparisonIndex << '\n';

					if (!assetpathname.ComparisonIndex)
						continue;

					auto VehicleActorClass = load(Helper::GetBGAClass(), assetpathname.ToString());

					std::cout << "VehicleActorClass: " << VehicleActorClass << '\n';

					if (!VehicleActorClass)
						continue;

					auto SpawnerLoc = Helper::GetActorLocation(Spawner);

					Helper::Easy::SpawnActor(VehicleActorClass, SpawnerLoc, Helper::GetActorRotation(Spawner));
				}
			}
			else
			{
				static auto FortVehicleItemDefOffset = Spawner->GetOffset("FortVehicleItemDef");

				auto FortVehicleItemDefSoft = Get<TSoftObjectPtr>(Spawner, FortVehicleItemDefOffset);

				auto assstpaht = FortVehicleItemDefSoft->ObjectID.AssetPathName;

				std::cout << "assstpaht: " << assstpaht.ComparisonIndex << '\n';

				if (!assstpaht.ComparisonIndex)
					continue;

				auto FortVehicleItemDef = load(VIDClass, assstpaht.ToString());

				if (!FortVehicleItemDef)
					continue;

				static auto VehicleActorClassOffset = FortVehicleItemDef->GetOffset("VehicleActorClass");

				auto VehicleActorClassSoft = Get<TSoftObjectPtr>(FortVehicleItemDef, VehicleActorClassOffset);

				auto assetpathname = VehicleActorClassSoft->ObjectID.AssetPathName;

				std::cout << "assetpathname sof: " << assetpathname.ComparisonIndex << '\n';

				if (!assetpathname.ComparisonIndex)
					continue;

				auto VehicleActorClass = load(Helper::GetBGAClass(), assetpathname.ToString());

				std::cout << "VehicleActorClass: " << VehicleActorClass << '\n';

				if (!VehicleActorClass)
					continue;

				auto SpawnerLoc = Helper::GetActorLocation(Spawner);

				Helper::Easy::SpawnActor(VehicleActorClass, SpawnerLoc, Helper::GetActorRotation(Spawner));
			}
		}

		Spawners.Free();
	}

	if (Defines::bShouldSpawnFloorLoot && Engine_Version >= 421) // TODO move this
	{
		static auto SpawnIsland_FloorLoot = FindObject("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C");
		static auto BRIsland_FloorLoot = FindObject("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");

		if (!SpawnIsland_FloorLoot || !BRIsland_FloorLoot) // Map has not loaded to the point where there are floor loot actors
		{
			SpawnIsland_FloorLoot = FindObject("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C");
			BRIsland_FloorLoot = FindObject("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C");
		}
		else
		{
			std::cout << "Spawning floor loot!\n";

			std::cout << "SpawnIsland_FloorLoot: " << SpawnIsland_FloorLoot << '\n';
			std::cout << "BRIsland_FloorLoot: " << BRIsland_FloorLoot << '\n';

			auto SpawnFloorLoot = [](UObject* Class) -> int
			{
				auto ClassActors = Helper::GetAllActorsOfClass(Class);

				std::cout << "Size; " << ClassActors.Num() << '\n';

				for (int i = 0; i < ClassActors.Num(); i++)
				{
					auto ClassActor = ClassActors.At(i);

					if (ClassActor)
					{
						auto CorrectLocation = Helper::GetActorLocation(ClassActor);
						CorrectLocation.Z += 50;

						bool ShouldSpawn = RandomBoolWithWeight(0.3f);

						if (ShouldSpawn)
						{
							auto CorrectLocation = Helper::GetActorLocation(ClassActor);
							CorrectLocation.Z += 50;

							UObject* MainPickup = nullptr;

							if (RandomBoolWithWeight(6, 1, 100))
							{
								auto Ammo = Looting::GetRandomItem(ItemType::Ammo);

								MainPickup = Helper::SummonPickup(nullptr, Ammo.Definition, CorrectLocation, EFortPickupSourceTypeFlag::FloorLoot,
									EFortPickupSpawnSource::Unset, Ammo.DropCount);
							}

							else if (RandomBoolWithWeight(5, 1, 100))
							{
								auto Trap = Looting::GetRandomItem(ItemType::Trap);

								MainPickup = Helper::SummonPickup(nullptr, Trap.Definition, CorrectLocation, EFortPickupSourceTypeFlag::FloorLoot,
									EFortPickupSpawnSource::Unset, Trap.DropCount);
							}

							else if (RandomBoolWithWeight(26, 1, 100))
							{
								auto Consumable = Looting::GetRandomItem(ItemType::Consumable);

								MainPickup = Helper::SummonPickup(nullptr, Consumable.Definition, CorrectLocation, EFortPickupSourceTypeFlag::FloorLoot,
									EFortPickupSpawnSource::Unset, Consumable.DropCount);
							}

							else
							{
								auto Weapon = Looting::GetRandomItem(ItemType::Weapon);

								MainPickup = Helper::SummonPickup(nullptr, Weapon.Definition, CorrectLocation, EFortPickupSourceTypeFlag::FloorLoot, EFortPickupSpawnSource::Unset, 1, true);

								if (MainPickup)
								{
									auto AmmoDef = Helper::GetAmmoForDefinition(Weapon.Definition);

									Helper::SummonPickup(nullptr, AmmoDef.first, CorrectLocation, EFortPickupSourceTypeFlag::FloorLoot,
										EFortPickupSpawnSource::Unset, AmmoDef.second);
								}
							}
						}
					}
				}

				int Num = ClassActors.Num();

				ClassActors.Free();

				return Num;
			};

			if (SpawnFloorLoot(SpawnIsland_FloorLoot) != 0)
			{
				if (SpawnFloorLoot(BRIsland_FloorLoot) != 0)
				{
					Defines::bShouldSpawnFloorLoot = false;
				}
			}
		}
	}

	return Defines::TickFlush(thisNetDriver, DeltaSeconds);
}

void Server::Hooks::KickPlayer(UObject* GameSession, UObject* Controller, FText a3)
{
	std::cout << "KickPlayer!\n";
	return;
}

char Server::Hooks::ValidationFailure(__int64* a1, __int64 a2)
{
	std::cout << "Validation Failure!\n";
	return false;
}

__int64 Server::Hooks::NoReservation(__int64* a1, __int64 a2, char a3, __int64 a4)
{
	std::cout << "No Reserve!\n";
	return 0;
}

__int64 __fastcall Server::Hooks::NetViewerConstructorDetour(__int64 NetViewer, UObject* Connection)
{
	static auto Connection_ViewTargetOffset = Connection->GetOffset("ViewTarget");
	static auto Connection_PlayerControllerOffset = Connection->GetOffset("PlayerController");
	static auto Connection_OwningActorOffset = Connection->GetOffset("OwningActor");

	auto Connection_ViewTarget = *(UObject**)(__int64(Connection) + Connection_ViewTargetOffset);
	auto Connection_PlayerController = *(UObject**)(__int64(Connection) + Connection_PlayerControllerOffset);

	static auto Viewer_ConnectionOffset = FindOffsetStruct("ScriptStruct /Script/Engine.NetViewer", "Connection");
	*(UObject**)(__int64(NetViewer) + Viewer_ConnectionOffset) = Connection;

	static auto Viewer_InViewerOffset = FindOffsetStruct("ScriptStruct /Script/Engine.NetViewer", "InViewer");
	*(UObject**)(__int64(NetViewer) + Viewer_InViewerOffset) = Connection_PlayerController ? Connection_PlayerController : *(UObject**)(__int64(Connection) + Connection_OwningActorOffset);

	static auto Viewer_ViewTargetOffset = FindOffsetStruct("ScriptStruct /Script/Engine.NetViewer", "ViewTarget");
	auto Viewer_ViewTarget = (UObject**)(__int64(NetViewer) + Viewer_ViewTargetOffset);
	*Viewer_ViewTarget = Connection_ViewTarget;

	static auto Viewer_ViewLocationOffset = FindOffsetStruct("ScriptStruct /Script/Engine.NetViewer", "ViewLocation");
	auto Viewer_ViewLocation = (FVector*)(__int64(NetViewer) + Viewer_ViewLocationOffset);

	if (*Viewer_ViewTarget)
		*(FVector*)(__int64(NetViewer) + Viewer_ViewLocationOffset) = Helper::GetActorLocation(*Viewer_ViewTarget);

	float CP, SP, CY, SY;

	FRotator ViewRotation = (*Viewer_ViewTarget) ? Helper::GetActorRotation(*Viewer_ViewTarget) : FRotator();

	SinCos(&SP, &CP, DegreesToRadians(ViewRotation.Pitch));
	SinCos(&SY, &CY, DegreesToRadians(ViewRotation.Yaw));

	static auto Viewer_ViewDirOffset = FindOffsetStruct("ScriptStruct /Script/Engine.NetViewer", "ViewDir");

	*(FVector*)(__int64(NetViewer) + Viewer_ViewDirOffset) = FVector(CP * CY, CP * SY, SP);

	return NetViewer;
}