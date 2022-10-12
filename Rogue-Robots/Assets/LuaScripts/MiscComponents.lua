require("VectorMath")

MiscComponent = {}

function MiscComponent.NormalGun()
	return {
		cooldown = 0,

		Update = function(self, parentEntity) 
			self.cooldown = self.cooldown - DeltaTime
			if self.cooldown <= 0.0 and Entity:GetAction(parentEntity, 1) then
				self.cooldown = 0.1
				
				local pos = Vector3.FromTable(Entity:GetTransformPosData(parentEntity))
				local forward = Vector3.FromTable(Entity:GetForward(parentEntity))
				local up = Vector3.FromTable(Entity:GetUp(parentEntity))
				local right = Vector3.FromTable(Entity:GetRight(parentEntity))

				pos = pos + up * -0.14 + right * 0.2 + forward * 0.6

				local bullet = {
					entity = 0,
					forward = forward,
					startPos = pos,
					speed = 75.0,
					size = Vector3.New(3, 3, 3),
					lifetime = 0
				}

				return bullet 
			end
			return nil
		end
	}
end

function MiscComponent.ChargeShot()
	return {
		shotPower = 0.0,
		chargeSpeed = 10.0,
		pressing = false,

		Update = function(self, parentEntity)
			
			if Entity:GetAction(parentEntity, 1) then
				self.pressing = true
				self.shotPower = self.shotPower + self.chargeSpeed * DeltaTime

			elseif self.pressing then
				self.pressing = false
				
				local pos = Vector3.FromTable(Entity:GetTransformPosData(parentEntity))
				local forward = Vector3.FromTable(Entity:GetForward(parentEntity))
				local up = Vector3.FromTable(Entity:GetUp(parentEntity))
				local right = Vector3.FromTable(Entity:GetRight(parentEntity))

				pos = pos + up * -0.14 + right * 0.2 + forward * 0.6

				local bullet = {
					entity = 0,
					forward = forward,
					startPos = pos,
					speed = 75.0,
					size = Vector3.New(2, 2, 2) + Vector3.New(self.shotPower, self.shotPower, self.shotPower),
					lifetime = 0
				}

				self.shotPower = 0.0

				return bullet

			end
			return nil
		end
	}
end

return MiscComponent

