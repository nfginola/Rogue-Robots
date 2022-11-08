#pragma once
#include <DOGEngine.h>

std::vector<DOG::entity> SpawnPlayers(const DirectX::SimpleMath::Vector3& pos, u8 playerCount, f32 spread = 10.f);

std::vector<DOG::entity> AddFlashlightsToPlayers(const std::vector<DOG::entity>& players);