require("VectorMath")
require("MiscComponents")

--Managers for objects & component functions.
local ObjectManager = require("Object")
local BarrelManager = require("BarrelComponents")
local MagazineManager = require("MagazineComponents")

--Tweakable values.
local InitialBulletSpeed = 75.0
local ShootCooldown = 0.1
local BulletDespawnDist = 50
local BulletSize = Vector3.New(3, 3, 3)

local savedBulletCount = 30
local currentAmmoCount = nil
local hasBasicBarrelEquipped = true

--The 3 different components.
--The pipeline is: misc -> barrel -> magazine.
local barrelComponent = nil
local magazineComponent = nil
local miscComponent = nil

--A template. Not supposed to be used, simply here to show what variables exist to be used.
local bulletTemplate = {
	entity = 0,					-- ID used by the ECS.
	forward = {},				-- Vector3 that describes the direction of the bullet.
	startPos = {},				-- Vector3 that describes the initial spawn position of the bullet.
	speed = 0, -- Float that describes the current speed of the bullet. 
	lifetime = 0,				-- Counter to know when to kill the bullet entity.
	size = {},					-- Vector3 that describes the scale of the bullet.
}

--Non-tweakable
local gunModel = nil
local bulletModel = nil
local gunShotSound = nil
local bullets = {}
local shootTimer = 0.0

gunEntity = {
	entityID = nil,
	position = Vector3.Zero(),
	rotation = Vector3.Zero(),
}
barrelEntityID = 0
miscEntityID = 0
magazineEntityID = 0
lightEntityID = 0

local basicBarrelEquiped = true
--Ammo and reloading 
local maxAmmo = 30
local currentAmmo = 30
local ammoLeft = -1
local reloadTimer = 0.0
local reloading = false
local reloadAngle = 0.0

function OnStart()
	gunModel = Asset:LoadModel("Assets/Models/ModularRifle/Maingun.gltf")
	bulletModel = Asset:LoadModel("Assets/Models/Ammunition/Bullet/556x45_bullet.fbx")
	gunShotSound = Asset:LoadAudio("Assets/Audio/TestShoot.wav")

	-- Initialize the gun view model entity
	gunID = Scene:CreateEntity(EntityID)
	gunEntity.entityID = gunID
	Entity:AddComponent(gunID, "Transform", gunEntity.position, gunEntity.rotation, {x=.35,y=.35,z=.35})
	Entity:AddComponent(gunID, "Model", gunModel)
	Entity:AddComponent(gunID, "Audio", gunShotSound, false, true)

	barrelEntityID = Scene:CreateEntity(gunID)
	Entity:AddComponent(barrelEntityID, "Transform", Vector3:Zero(), Vector3:Zero(), Vector3:One())
	Entity:AddComponent(barrelEntityID, "Child", gunID, Vector3.Zero(), Vector3.Zero(), Vector3.One())
	miscEntityID = Scene:CreateEntity(gunID)
	Entity:AddComponent(miscEntityID, "Transform", Vector3:Zero(), Vector3:Zero(), Vector3:One())
	Entity:AddComponent(miscEntityID, "Child", gunID, Vector3.Zero(), Vector3.Zero(), Vector3.One())
	magazineEntityID = Scene:CreateEntity(gunID)
	Entity:AddComponent(magazineEntityID, "Transform", Vector3:Zero(), Vector3:Zero(), Vector3:One())
	Entity:AddComponent(magazineEntityID, "Child", gunID, Vector3.Zero(), Vector3.Zero(), Vector3.One())

	local outlineColor = Entity:GetOutlineColor(EntityID)
	-- Add shit
	if (Entity:HasComponent(EntityID, "ThisPlayer")) then

		Entity:AddComponent(gunID, "ThisPlayerWeapon")
		--To be fixed later hopefully
		Entity:AddComponent(barrelEntityID, "ThisPlayerWeapon")
		Entity:AddComponent(miscEntityID, "ThisPlayerWeapon")
		Entity:AddComponent(magazineEntityID, "ThisPlayerWeapon")
		CreateWeaponLights()
	end

	Entity:AddComponent(gunID, "OutlineComponent", outlineColor.r, outlineColor.g, outlineColor.b)
	Entity:AddComponent(barrelEntityID, "OutlineComponent", outlineColor.r, outlineColor.g, outlineColor.b)
	Entity:AddComponent(miscEntityID, "OutlineComponent", outlineColor.r, outlineColor.g, outlineColor.b)
	Entity:AddComponent(magazineEntityID, "OutlineComponent", outlineColor.r, outlineColor.g, outlineColor.b)

	-- Initialize base components
	miscComponent = MiscComponent.BasicShot()
	barrelComponent = BarrelManager.BasicBarrel() 
	magazineComponent = MagazineManager.BasicEffect() --ObjectManager:CreateObject()

	currentAmmoCount = barrelComponent:GetMaxAmmo()

	EventSystem:Register("ItemPickup" .. EntityID, OnPickup)
end

local tempMode = 0
local tempTimer = 0.0
function OnUpdate()
	-- Update gun model position
	local cameraEntity = Entity:GetPlayerControllerCamera(EntityID)

	gunEntity.position = Vector3.FromTable(Entity:GetTransformPosData(cameraEntity))
	local playerUp = Vector3.FromTable(Entity:GetUp(cameraEntity))
	local playerForward = Vector3.FromTable(Entity:GetForward(cameraEntity))
	local playerRight = Vector3.FromTable(Entity:GetRight(cameraEntity))

	-- Move gun down and to the right 
	gunEntity.position = gunEntity.position + playerRight * 0.3 - playerUp * 0.15 + playerForward * 0.4

	-- Rotate the weapon by 90 degrees pitch
	local angle = math.pi / 2 + math.pi / 150.0 
	local gunForward = RotateAroundAxis(playerForward, playerUp, angle)
	local gunUp = playerUp--RotateAroundAxis(playerUp, playerRight, angle)

	Entity:SetRotationForwardUp(gunEntity.entityID, gunForward, gunUp)
	Entity:ModifyComponent(gunEntity.entityID, "Transform", gunEntity.position, 1)

	NormalBulletUpdate() -- Should be called something else, but necessary for bullet despawn

	if hasBasicBarrelEquipped then
		if (ReloadSystem()) then
			return
		end
	end

	-- Gun firing logic
	if miscComponent.miscName == "FullAuto" and barrelComponent.GetECSType() == 3 then
		-- Handle the laser barrel component in FullAuto outside MiscComponent.Update and BarrelComponent.Update.
		local shoot = Entity:GetAction(EntityID, "Shoot")
		local dir = Vector3.FromTable(Entity:GetRight(gunEntity.entityID))
		dir = Norm(dir)
		local laserStart = GetPositionToSpawn(cameraEntity, -0.175, 0.31, 0.05)
		laserStart = laserStart + dir * 0.8
		local color = Vector3.New(1.5, 0.1, 0.1) * 7
		if magazineComponent:GetECSType() == 1 then
			color = Vector3.New(0.188, 0.835, 0.784) * 7 -- Blue color for FrostEffect
		elseif magazineComponent:GetECSType() == 2 then
			color = MaterialPrefabs:GetMaterial("FireExplosionMaterial")["emissiveFactor"]
		end

		local isOutOfAmmo = Entity:ModifyComponent(EntityID, "LaserBarrel", EntityID, 80.0, 700.0, shoot, laserStart, dir, color)
		if isOutOfAmmo then
			barrelComponent = BarrelManager.BasicBarrel()
			Entity:RemoveComponent(barrelEntityID, "Model")
			Entity:RemoveComponent(EntityID, "BarrelComponent")
			Entity:AddComponent(EntityID, "BarrelComponent", 0, 30, 999999)
			currentAmmoCount = savedBulletCount
			hasBasicBarrelEquipped = true
			Entity:UpdateMagazine(EntityID, currentAmmoCount)
		end
	else
		-- Returns a table of bullets
		local newBullets = miscComponent:Update(EntityID, cameraEntity)
		if not newBullets then
			return
		end
		for i=1, #newBullets do
			
			local createBullet = true
			if barrelComponent.CreateBullet then
				createBullet = barrelComponent:CreateBullet(miscComponent)
			end
			if createBullet and currentAmmoCount > 0 then

				if hasBasicBarrelEquipped then
					currentAmmo = currentAmmo - 1
				end

				CreateBulletEntity(newBullets[i], cameraEntity)
				barrelComponent:Update(gunEntity, EntityID, newBullets[i], miscComponent, cameraEntity)
				--Keep track of which barrel created the bullet
				newBullets[i].barrel = barrelComponent
				magazineComponent:Update(newBullets[i], EntityID)
				
				currentAmmoCount = currentAmmoCount - 1
				if currentAmmoCount == 0 and not hasBasicBarrelEquipped then
					barrelComponent = BarrelManager.BasicBarrel()
					Entity:RemoveComponent(barrelEntityID, "Model")
					Entity:RemoveComponent(EntityID, "BarrelComponent")
					Entity:AddComponent(EntityID, "BarrelComponent", 0, 30, 999999)
					currentAmmoCount = savedBulletCount
					hasBasicBarrelEquipped = true
				end
				Entity:UpdateMagazine(EntityID, currentAmmoCount)
			end
		end
	end
end

function OnDestroy()
	EventSystem:UnRegister("ItemPickup" .. EntityID, OnPickup)
	Entity:DestroyEntity(gunEntity.entityID)
	Entity:DestroyEntity(barrelEntityID)
	Entity:DestroyEntity(miscEntityID)
	Entity:DestroyEntity(magazineEntityID)
	DestroyWeaponLights()
end

function CreateBulletEntity(bullet, transformEntity)
	bullet.entity = Scene:CreateEntity(EntityID)

	table.insert(bullets, bullet)

	size = Vector3.New(1.0, 1.0, 1.0)
	if Vector3.Zero == bullet.size then
		size = bullet.size
	end

	Entity:AddComponent(bullet.entity, "Transform",
		Vector3.Zero(),
		Vector3.Zero(),
		size--bullet.size
	)
	local up = Vector3.FromTable(Entity:GetUp(gunEntity.entityID))
	local angle = math.pi

	local newForward = RotateAroundAxis(Entity:GetForward(gunEntity.entityID), up, angle)
	Entity:SetRotationForwardUp(bullet.entity, newForward, up)

	Entity:ModifyComponent(bullet.entity, "Transform", bullet.startPos, 1)
end

function ReloadSystem()
	--When reloading
	if reloadTimer >= ElapsedTime then
		--Reloading Animation 
		reloadAngle = reloadAngle + 2.0 * math.pi * DeltaTime / barrelComponent:GetReloadTime()

		local gunUp = Vector3.FromTable(Entity:GetUp(gunEntity.entityID))
		local gunForward = Vector3.FromTable(Entity:GetForward(gunEntity.entityID))
		local gunRight = Vector3.FromTable(Entity:GetRight(gunEntity.entityID))

		local newGunForward = RotateAroundAxis(gunForward, gunRight, reloadAngle)
		local newGunUp = RotateAroundAxis(gunUp, gunRight, reloadAngle)

		Entity:SetRotationForwardUp(gunEntity.entityID, newGunForward, newGunUp)
		Entity:ModifyComponent(gunEntity.entityID, "Transform", gunEntity.position, 1)

		return true
	end
	reloadAngle = 0.0

	if reloading then
		reloading = false
		local reloadAmount = maxAmmo - currentAmmo
		ammoLeft = ammoLeft - reloadAmount
		if ammoLeft < 0 then
			reloadAmount = ammoLeft + reloadAmount
			ammoLeft = 0
		end
		currentAmmo = currentAmmo + reloadAmount

		if hasBasicBarrelEquipped then
			currentAmmo = maxAmmo
			ammoLeft = -1
			Entity:UpdateMagazine(EntityID, maxAmmo)
			currentAmmoCount = maxAmmo
		end
	end

	if Entity:GetAction(EntityID, "Reload") and currentAmmo < maxAmmo and (ammoLeft > 0 or ammoLeft == -1) then
		reloadTimer = barrelComponent:GetReloadTime() + ElapsedTime
		reloading = true
		return true
	end

	return false
end

--If there is not barrel component start.
function NormalBulletSpawn(bullet, transformEntity)
	bullet.entity = Scene:CreateEntity(EntityID)
	table.insert(bullets, bullet)

	Entity:AddComponent(bullet.entity, "Transform",
		Vector3.Zero(),
		Vector3.Zero(),
		bullet.size
	)
	
	local up = Vector3.FromTable(Entity:GetUp(transformEntity))
	local angle = -math.pi / 2

	local newForward = RotateAroundAxis(Entity:GetForward(transformEntity), up, angle)
	Entity:SetRotationForwardUp(bullet.entity, newForward, up)
	Entity:ModifyComponent(bullet.entity, "Transform", bullet.startPos, 1)

	Entity:AddComponent(bullet.entity, "Model", bulletModel)
	Entity:AddComponent(bullet.entity, "BoxCollider", Vector3.New(.1, .1, .1), true)
	Entity:AddComponent(bullet.entity, "Rigidbody", false)
	Entity:AddComponent(bullet.entity, "Bullet", EntityID)		-- Note: bullet damage is added in Lua interface

	Entity:PlayAudio(gunEntity.entityID, gunShotSound, true)

	Physics:RBSetVelocity(bullet.entity, bullet.forward * bullet.speed)
end

--If there is no barrel component update.
function NormalBulletUpdate()
	for i = #bullets, 1, -1 do -- Iterate through bullets backwards to make removal of elements safe
		bullets[i].lifetime = bullets[i].lifetime + DeltaTime
		if bullets[i].lifetime > bullets[i].barrel:DestroyTime() then
			bullets[i].barrel:Destroy(bullets[i], EntityID)
			Entity:DestroyEntity(bullets[i].entity)
			table.remove(bullets, i)
		end
	end
end

function OnPickup(pickup)

	--In order to adhere to network demands this is how it must behave:
	--pickup is an EntityType, NOT an ecs-entity.
	local pickupTypeString = Entity:GetEntityTypeAsString(pickup)
	local playerID = EntityID
	if pickupTypeString == "GrenadeBarrel" or pickupTypeString == "MissileBarrel" or pickupTypeString == "LaserBarrel" then
		--It is a barrel component:
		local typeOfEquippedBarrel = Entity:GetBarrelType(playerID)
		if pickupTypeString == typeOfEquippedBarrel then
			--Player essentially picked up ammo for already equipped barrel:
			if currentAmmoCount + barrelComponent:GetAmmoPerPickup() > barrelComponent:GetMaxAmmo() then
				currentAmmoCount = barrelComponent:GetMaxAmmo()
			else
				currentAmmoCount = currentAmmoCount + barrelComponent:GetAmmoPerPickup()
			end

			if pickupTypeString == "LaserBarrel" and miscComponent.miscName == "FullAuto" then -- LaserBeam ammo counting is handled on C++
				EventSystem:InvokeEvent("PickUpMoreLaserCharge", playerID)
			else
				Entity:UpdateMagazine(playerID, currentAmmoCount)
			end
		else
			if Entity:HasComponent(barrelEntityID, "Model") then
				Entity:RemoveComponent(barrelEntityID, "Model")
			end

			--Player picked up a new barrel type:
			if pickupTypeString == "GrenadeBarrel" then
				barrelComponent = BarrelManager.Grenade()
				Entity:AddComponent(barrelEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/Grenade.gltf"))
			elseif pickupTypeString == "MissileBarrel" then
				barrelComponent = BarrelManager.Missile()
				Entity:AddComponent(barrelEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/Missile.gltf"))
			elseif pickupTypeString == "LaserBarrel" then
				barrelComponent = BarrelManager.Laser()
				Entity:AddComponent(barrelEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/Laser.gltf"))
			end
			Entity:RemoveComponent(playerID, "BarrelComponent")
			Entity:AddComponent(playerID, "BarrelComponent", barrelComponent:GetECSType(), barrelComponent:GetAmmoPerPickup(), barrelComponent:GetMaxAmmo())

			if hasBasicBarrelEquipped == true then
				savedBulletCount = currentAmmoCount
			end
			currentAmmoCount = barrelComponent:GetAmmoPerPickup()
			
			hasBasicBarrelEquipped = false
		end
	elseif pickupTypeString == "FrostMagazineModification" or pickupTypeString == "FireMagazineModification" then
		--Magazine modification component
		local currentModificationType = Entity:GetModificationType(playerID)
		if pickupTypeString ~= currentModificationType then
			--A new magazine modification was picked up:
			Entity:RemoveComponent(playerID, "MagazineModificationComponent")

			if Entity:HasComponent(magazineEntityID, "Model") then
				Entity:RemoveComponent(magazineEntityID, "Model")
			end

			if pickupTypeString == "FrostMagazineModification" then
				magazineComponent = MagazineManager.FrostEffect()
				Entity:AddComponent(magazineEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/Frost.gltf"))
			elseif pickupTypeString == "FireMagazineModification" then
				magazineComponent = MagazineManager.FireEffect()
				Entity:AddComponent(magazineEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/Fire.gltf"))
			end
			Entity:AddComponent(playerID, "MagazineModificationComponent", magazineComponent:GetECSType())
		end	

	elseif pickupTypeString == "FullAutoMisc" or pickupTypeString == "ChargeShotMisc" then
		--Misc component
		local currentMiscType = Entity:GetMiscType(playerID)
		Game:SpawnPickupMiscComponent(playerID)
		if pickupTypeString ~= currentMiscType then
			--A new misc component was picked up:
			if Entity:HasComponent(miscEntityID, "Model") then
				Entity:RemoveComponent(miscEntityID, "Model")
			end

			Entity:RemoveComponent(playerID, "MiscComponent")
			if pickupTypeString == "FullAutoMisc" then
				miscComponent = MiscComponent.FullAuto()
				Entity:AddComponent(miscEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/FullAuto.gltf"))
			elseif pickupTypeString == "ChargeShotMisc" then
				miscComponent = MiscComponent.ChargeShot()
				Entity:AddComponent(miscEntityID, "Model", Asset:LoadModel("Assets/Models/ModularRifle/ChargeShot.gltf"))
			end
			Entity:AddComponent(playerID, "MiscComponent", miscComponent:GetECSType())
		end	
	end
end

function CreateWeaponLights()
	
	color = Vector3.New(1.0, 0.1, 0.1)
	playerColor = Game:GetPlayerName(EntityID)

	if playerColor == "Green" then
		color = Vector3.New(0.1, 1.0, 0.1)
	elseif playerColor == "Yellow" then
		color = Vector3.New(1.0, 1.0, 0.1)
	elseif playerColor == "Blue" then
		color = Vector3.New(0.1, 0.1, 1.0)
	end

	lightEntityID = Scene:CreateEntity(gunEntity.entityID)
	Entity:AddComponent(lightEntityID, "Transform", Vector3:Zero(), Vector3:Zero(), Vector3:One())
	Entity:AddComponent(lightEntityID, "PointLight", color, 0.2, 0.4)
	Entity:AddComponent(lightEntityID, "Child", gunEntity.entityID, Vector3.New(0.0, 0.0, 0.2), Vector3.Zero(), Vector3.New(0.1, 0.1, 0.1))
	Entity:AddComponent(lightEntityID, "WeaponLight")
end

function DestroyWeaponLights()
	if Entity:Exists(lightEntityID) then
		Entity:DestroyEntity(lightEntityID)
	end
end