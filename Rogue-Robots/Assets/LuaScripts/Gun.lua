--Tweakable values.
local MaxAmmo = 10
local InitialBulletSpeed = 10.0
local ShootCooldown = 0.1
local BulletDespawnDist = 50
local BulletSize = {
	x = 1.0,
	y = 1.0,
	z = 1.0
}

--Managers for objects & component functions.
local ObjectManager = require("Object")
local MiscManager = require("MiscComponents")
local BarrelManager = require("BarrelComponents")
local MagazineManager = require("MagazineComponents")

--The 3 different components.
--The pipeline is: misc -> barrel -> magazine.
local barrelComponent = nil
local magazineComponent = nil
local miscComponent = nil

--A template. Not supposed to be used, simply here to show what variables exist to be used.
local bulletTemplate = {
	entity = 0, --ID used by the ECS.
	forward = {}, --Vector3 that describes the direction of the bullet.
	startPos = {}, --Vector3 that describes the initial spawn position of the bullet.
	speed = InitialBulletSpeed --Float that describes the current speed of the bullet. 
}

--Non-tweakable
local gunModel = 0
local bulletModel = nil
local bullets = {}
local shootTimer = 0.0

local gunEntity = {
	entityID = nil,
	position = {x=0,y=0,z=0},
	rotation = {x=3.14/2,y=0,z=0}
}

function OnStart()
	gunModel = Asset:LoadModel("Assets/Rifle/scene.gltf")
	bulletModel = Asset:LoadModel("Assets/556x45_bullet.fbx")

	-- Initialize the gun view model entity
	gunID = Entity:CreateEntity()
	gunEntity.entityID = gunID
	Entity:AddComponent(gunID, "Transform", gunEntity.position, gunEntity.rotation, {x=.15,y=.15,z=.15})
	Entity:AddComponent(gunID, "Model", gunModel)

	barrelComponent = ObjectManager:CreateObject()
	magazineComponent = ObjectManager:CreateObject()
	miscComponent = ObjectManager:CreateObject()
	miscComponent.OnUpdate = MiscManager.ChargeShot

	--Events
	EventSystem:Register("NormalBulletUpdate", NormalBulletUpdate) --Is called if there is no barrelcomponent.
	EventSystem:Register("NormalBulletSpawn", NormalBulletSpawn) --Is called if there is no barrelcomponent.
end

local tempMode = 0
local tempTimer = 0.0
function OnUpdate()
	-- Update gun model position
	gunEntity.position = Player:GetPosition()
	up = Player:GetUp()
	pf = Player:GetForward()
	pr = Player:GetRight()
	
	gf = {x=-pf.x, y=-pf.y, z=-pf.z}
	gup = {x=up.x, y=up.y, z=up.z}

	pos = gunEntity.position
	gunEntity.position = {
		x = pos.x + 0.2 * pr.x - 0.2 * gup.x,
		y = pos.y - 0.2 * pr.y - 0.2 * gup.y, 
		z = pos.z + 0.2 * pr.z - 0.2 * gup.z
	}

	Entity:SetRotationForwardUp(gunEntity.entityID, gf, gup)

	Entity:ModifyComponent(gunEntity.entityID, "Transform", gunEntity.position, 1)

--Temporary code to allow for switching mode.
	tempTimer = tempTimer - DeltaTime
	if Input:IsKeyPressed("Q") and tempTimer <= 0.0 then
		if tempMode == 0 then
			miscComponent.OnUpdate = nil
			tempMode = 1
		else
			miscComponent.OnUpdate = MiscManager.ChargeShot
			tempMode = 0
		end
		tempTimer = 1.0
	end
----------------------------------------------------

	shootTimer = shootTimer - DeltaTime
	
	if miscComponent.OnUpdate then
		miscComponent:OnUpdate(barrelComponent, magazineComponent, bullets, InitialBulletSpeed, BulletSize)
	else
		NormalGunUpdate()
	end
end

--If there is no misc component.
function NormalGunUpdate()
	--If the shoot cooldown is up and we are clicking.
	if shootTimer < 0.0 and Input:IsLeftPressed() then
		shootTimer = ShootCooldown

		local bullet = {}
		bullet.entity = Entity:CreateEntity()
		bullet.forward = Player:GetForward()
		bullet.startPos = Player:GetPosition()
		bullet.speed = InitialBulletSpeed
		table.insert(bullets, bullet)

		Entity:AddComponent(bullet.entity, "Transform",{x = bullet.startPos.x, y = bullet.startPos.y, z = bullet.startPos.z}, {x = 0.0, y = 0.0, z = 0.0}, BulletSize)
		if barrelComponent.OnStart then
			barrelComponent:OnStart()
		else
			NormalBulletSpawn(bullet)
		end
	end
	
	if barrelComponent.OnUpdate then
		barrelComponent:OnUpdate()
	else
		NormalBulletUpdate()
	end
	if magazineComponent.OnUpdate then
		magazineComponent:OnUpdate()
	end
end

--If there is not barrel component start.
function NormalBulletSpawn(bullet)
	Entity:AddComponent(bullet.entity, "Model", bulletModel)
	Entity:AddComponent(bullet.entity, "Network")
end

--If there is no barrel component update.
function NormalBulletUpdate()
	local i = 1
	while i ~= #bullets + 1 do
		local t = Entity:GetTransformPosData(bullets[i].entity)
		local dist = math.sqrt((t.x - bullets[i].startPos.x)^2 + (t.y - bullets[i].startPos.y)^2 + (t.z - bullets[i].startPos.z)^2)
		if dist > BulletDespawnDist then
			Entity:DestroyEntity(bullets[i].entity)
			table.remove(bullets, i)
			i = i - 1
		else
			Entity:ModifyComponent(bullets[i].entity, "Transform", {x = t.x + bullets[i].speed * DeltaTime * -bullets[i].forward.x, y = t.y + bullets[i].speed * DeltaTime * -bullets[i].forward.y, z = t.z + bullets[i].speed * DeltaTime * -bullets[i].forward.z}, 1)
			bullets[i].speed = bullets[i].speed - 0.9 * DeltaTime --temp
		end

		i = i + 1
	end
end