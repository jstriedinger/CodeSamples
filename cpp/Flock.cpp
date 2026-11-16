//This file was created for a gameplay test
//Is a flock system meant to work with a custom 3D engine on cpp, this is not meant for Unreal engine, but pure cpp
//Test information can not be shared because of NDA

#include "pch.h"
#include "Flock.h"
#include <random>
#include <City.h>

namespace
{
	std::mt19937& Rng()
	{
		static std::mt19937 rng{ std::random_device{}() };
		return rng;
	}

	// Random integer in [0, maxExclusive)
	int RandomInt(int maxExclusive)
	{
		return std::uniform_int_distribution<int>(0, maxExclusive - 1)(Rng());
	}
}

Flock::Flock() = default;
Flock::~Flock() = default;

void Flock::OnInitialize(float radius, int numAgents)
{
	m_radius = radius;
	m_numAgents = numAgents;
	const float gapX = -12.0f + 8.0f * RandomInt(4);
	const float gapZ = 12.0f + 8.0f * RandomInt(4);
	m_origin = Vector3(gapX, RandomRange(5.0f, 12.0f), gapZ);


	m_agentDefenderShape = GetEngine().CreateBoxPrimitive(Vector3(m_radius * 2, m_radius * 2, m_radius * 2));
	m_agentNormalShape = GetEngine().CreateConePrimitive(m_radius * 2, m_radius * 2);
    CreateFlockAgents();
}


void Flock::CreateFlockAgents()
{
	m_flockAgents.clear();

	for (int i = 0; i < m_numAgents; ++i)
	{
		FlockAgent agent;

		agent.position = m_origin + Vector3(RandomRange(-5.0f, 5.0f),RandomRange(-5.0f, 5.0f),RandomRange(-5.0f, 5.0f));
		agent.radius = m_radius;
		agent.forward = Vector3(RandomRange(-1.0f, 1.0f), RandomRange(-0.3f, 0.3f), RandomRange(-1.0f, 1.0f));
		agent.forward.Normalize();
		agent.velocity = agent.forward * RandomRange(m_minSpeed, m_minSpeed + 1.0f);
		m_flockAgents.push_back(agent);
	}
}

void Flock::OnUpdate(float deltaTime, const std::vector<Skyscraper>& buildings, Floor& floor)
{
	for (FlockAgent& agent : m_flockAgents)
	{
		//for each flock we calculate alignment, cohesion and separation
		//each gives us a normalized direction and we multiply it by its weight
		//The weights are tuned by hand until the flock feels right
		Vector3 flockDir = Vector3::Zero;
		CalculateFlockDirection(agent, flockDir);

		//Keeps the flocks inside the play area
		CalculateContainmentDirection(agent, flockDir);

		//Avoidance to obstacles
		CalculateCollisionDirection(agent, buildings, floor, flockDir);

		//normal agents escape from the red ball, defenders pursue it
		if (agent.type == FlockAgentType::Normal)
			CalculateScatterDirection(agent, flockDir);
		else if (agent.type == FlockAgentType::Defender)
			CalculateInterceptionDirection(agent, flockDir);

		flockDir.Normalize();	


		const bool isChasing = agent.type == FlockAgentType::Defender && agent.hasThreat;
		const float accel = isChasing ? m_defenderAcceleration : m_acceleration;
		const float maxSpeed = isChasing ? m_defenderMaxSpeed : m_maxSpeed;

		agent.velocity += flockDir * accel * deltaTime;
		const float speed = agent.velocity.Length();
		if (speed > maxSpeed)
			agent.velocity *= maxSpeed / speed;
		else if (speed > 0.0001f && speed < m_minSpeed)
			agent.velocity *= m_minSpeed / speed;

		agent.position += agent.velocity * deltaTime;
		agent.forward = agent.velocity;
		agent.forward.Normalize();
	}

}

void Flock::OnRender(cdp_framework::RenderContextPtr& renderContext)
{
	for (FlockAgent& agent : m_flockAgents)
	{
		if (agent.type == FlockAgentType::Defender)
			renderContext->RenderPrimitive(m_agentDefenderShape, Vector3::One, agent.position, Vector3::Zero, m_agentDefenderColor);
		else
		{
			const float yaw = std::atan2(agent.forward.x, agent.forward.z);
			const float y = std::max(-1.0f, std::min(1.0f, agent.forward.y));
			const float pitch = std::acos(y);
			renderContext->RenderPrimitive(m_agentNormalShape, Vector3::One, agent.position,
				Vector3(yaw, pitch, 0.0f), m_agentNormalColor);

		}
	}
}

float Flock::RandomRange(float minValue, float maxValue)
{
	return std::uniform_real_distribution<float>(minValue, maxValue)(Rng());
}

//Alignment, cohesion and separation!
void Flock::CalculateFlockDirection(FlockAgent& agent, Vector3& finalDir)
{
	int numNeighbors = 0;
	Vector3 alignment, cohesion, separation = Vector3::Zero;

	for (FlockAgent& neighbor : m_flockAgents)
	{
		if (&neighbor == &agent) continue;

		Vector3 neighborPos = neighbor.position;
		Vector3 toNeighbor = neighborPos - agent.position;
		float distSqr = toNeighbor.LengthSquared();


		const float reach = m_neighborRadius + agent.radius + neighbor.radius;
		const float separationReach = m_separationRadius + agent.radius + neighbor.radius;

		if (distSqr <= separationReach * separationReach && distSqr > 0.0f)
		{
			//stronger when closer, weaker when farther away
			separation += (agent.position - neighborPos) / distSqr;
		}

		if (distSqr <= reach * reach && distSqr > 0.0f)
		{
			alignment += neighbor.velocity;
			cohesion += neighborPos;
			numNeighbors++;
		}

		//with hundreds of agents, stop once we have enough neighbors
		if (numNeighbors >= m_maxNeighbors)
			break;

	}
	if (numNeighbors > 0)
	{
		alignment /= numNeighbors;     
		alignment.Normalize();	
		alignment *= m_alignmentWeight; 

		cohesion = (cohesion / numNeighbors) - agent.position;
		cohesion.Normalize();
		cohesion *= m_cohesionWeight;

		separation *= m_separationWeight;

		finalDir = (alignment + cohesion + separation);
	}
	else
	{
		//if we have no neighbors we just go forward
		finalDir = agent.forward;
	}
	
}

//Pulls the agent back toward the nearest point inside the bounds (hardcoded to a box)
void Flock::CalculateContainmentDirection(FlockAgent& agent, Vector3& finalDir)
{
	Vector3 closest;
	closest.x = std::max(m_boundsMin.x, std::min(agent.position.x, m_boundsMax.x));
	closest.y = std::max(m_boundsMin.y, std::min(agent.position.y, m_boundsMax.y));
	closest.z = std::max(m_boundsMin.z, std::min(agent.position.z, m_boundsMax.z));

	finalDir += (closest - agent.position) * m_containmentWeight;   
}

void Flock::CalculateCollisionDirection(FlockAgent& agent, const std::vector<Skyscraper>& buildings, Floor& floor, Vector3& finalDir)
{
	const float checkRadius = agent.radius * 3.0f;

	float   nearestAhead = m_lookAheadDistance;  
	Vector3 nearestLateral = Vector3::Zero;
	bool    willCollide = false;

	//now we test against every building
	for (const Skyscraper& s : buildings)
	{
		//find the closest point on this building to our agent
		const Vector3 halfSize(s.width * 0.5f, s.height * 0.5f, s.length * 0.5f);
		const Vector3 boxMin = s.position - halfSize;
		const Vector3 boxMax = s.position + halfSize;

		Vector3 closestPoint;
		closestPoint.x = std::max(boxMin.x, std::min(agent.position.x, boxMax.x));
		closestPoint.y = std::max(boxMin.y, std::min(agent.position.y, boxMax.y));
		closestPoint.z = std::max(boxMin.z, std::min(agent.position.z, boxMax.z));


		Vector3 toObstacle = closestPoint - agent.position;
		float distanceToObstacleAhead = toObstacle.Dot(agent.forward);

		//Make sure it is not behind us or too far away
		if (distanceToObstacleAhead < 0.0f || distanceToObstacleAhead > m_lookAheadDistance)
			continue; 

		//obstacle avoidance as described by Craig Reynolds
		const Vector3 lateralOffset = toObstacle - (agent.forward * distanceToObstacleAhead);
		//Like testing a cylinder of radius checkRadius and length m_lookAheadDistance
		if (lateralOffset.LengthSquared() > checkRadius * checkRadius) continue;


		//we only react to the closest obstacle
		if (distanceToObstacleAhead < nearestAhead)         
		{
			nearestAhead = distanceToObstacleAhead;
			nearestLateral = lateralOffset;
			willCollide = true;
		}


		
	}
	if (willCollide)
	{
		Vector3 steer = -nearestLateral;
		steer.Normalize();
		finalDir += steer * m_avoidanceWeight;
	}

	//now the floor, which is simpler
	const float futureY = agent.position.y + agent.forward.y * m_lookAheadDistance;
	if (futureY < floor.height + checkRadius)
	{
		finalDir += Vector3::UnitY * m_avoidanceWeight;
	}

}

void Flock::CalculateScatterDirection(FlockAgent& agent, Vector3& finalDir)
{
	if (agent.hasThreat)
	{
		//escaping from the red ball
		Vector3 threatToAgent = agent.position - agent.threatPos;
		Vector3 dir;

		agent.threatVelocity.Normalize(dir);
		float along = threatToAgent.Dot(dir);

		//closest point
		const Vector3 closestOnTargetPath = agent.threatPos + dir * along;
		//escape perpendicular to the ball's path
		Vector3 escape = agent.position - closestOnTargetPath;

		//if the agent is almost exactly on the red ball's path
		if (escape.LengthSquared() < 0.01f)
			escape = dir.Cross(Vector3::UnitY);

		escape.Normalize();
		finalDir += escape * m_scatterWeight;
	}

}

//Mirror of scatter: instead of escaping, the defender heads toward the threat
void Flock::CalculateInterceptionDirection(FlockAgent& agent, Vector3& finalDir)
{
	if (agent.hasThreat)
	{
		//go straight for the red ball
		Vector3 pursue = agent.threatPos - agent.position;
		pursue.Normalize();
		finalDir += pursue * m_scatterWeight;
	}
}

void Flock::OnShutdown()
{
	m_flockAgents.clear();
	m_agentNormalShape.reset();
}

void Flock::AddFlockAgent(Vector3 origin, Vector3 velocity, FlockAgentType type)
{
	FlockAgent agent;
	agent.type = type;
	agent.position = origin;
	agent.radius = m_radius;
	agent.velocity = velocity;
	agent.forward = velocity;
	agent.forward.Normalize();
	m_flockAgents.push_back(agent);
	m_numAgents = m_flockAgents.size();
	if (type == FlockAgentType::Defender)
	{
		m_numDefenders++;
	}

}



void Flock::UpdateGameState()
{
	m_flockAgents.erase(
		std::remove_if(m_flockAgents.begin(), m_flockAgents.end(),
			[this](const FlockAgent& agent) {
				return agent.dead;
			}),
		m_flockAgents.end());

	//Now we check if we need to add a defender agent
	if (m_numEatenByRedball >= m_maxEatenBeforeDefender && !m_flockAgents.empty())
	{
		//spawn next to a random existing agent
		const FlockAgent& randomParent = m_flockAgents[RandomInt(static_cast<int>(m_flockAgents.size()))];
		Vector3 randomPos = randomParent.position + Vector3(RandomRange(-2.0f, 2.0f), RandomRange(-2.0f, 2.0f), RandomRange(-2.0f, 2.0f));
		AddFlockAgent(randomPos, Vector3::Zero, FlockAgentType::Defender );
		
		m_numEatenByRedball = 0; //reset counter
	}
}

void Flock::OnFlockAgentCollidesWithRedball(FlockAgent* agent)
{
	if (!agent->dead)
	{
		agent->dead = true;
		if (agent->type == FlockAgentType::Normal)
			m_numEatenByRedball++;
		else if (agent->type == FlockAgentType::Defender)
			m_numDefenders--;
	}

}
