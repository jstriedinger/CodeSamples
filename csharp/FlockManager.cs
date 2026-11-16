public class FlockManager : Cullable
{
	[Header("Job")]
	private NativeArray<BoidData> _agentsDataArray;
	private NativeArray<float2> _steeringOutputArray;
	private NativeArray<float> _speedMultiplierOutput, _currentSpeedMultiplier;

	[Title("Main config")]
	[SerializeField] private bool isActive = true;
	[SerializeField] private FlockBehaviorSO flockBehavior;
	[SerializeField] private FlockAgentJob agentPrefab;
	[SerializeField] private Transform spawnPoint;
	[Range(2, 500)]
	public int numAgents = 250;
	[Range(1f, 100f)]
	public float acceleration = 10f;


	//--- behaviors stuff
	private FlockBehaviorBase[] _behaviors;
	private readonly List<FlockAgentJob> _agents = new List<FlockAgentJob>();
	public Transform[] agentTransforms { get; private set; }

	//Keep tracking
	private Dictionary<Collider2D, FlockAgentJob> _agentColliderMap = new();

	private void Awake()
	{
		// Get all behaviors attached as components
		_behaviors = GetComponents<FlockBehaviorBase>();
	}

	private void Start()
	{
		SetupFlockJobData();
		InitializeAgents();
	}

	//Necessary setup for flock properties, meaning stuff all agents will share
	private void SetupFlockJobData()
	{
		//initializing native arrays for job
		_agentsDataArray = new NativeArray<BoidData>(numAgents, Allocator.Persistent);
		_steeringOutputArray = new NativeArray<float2>(numAgents, Allocator.Persistent);
		_speedMultiplierOutput = new NativeArray<float>(numAgents, Allocator.Persistent);
		_currentSpeedMultiplier = new NativeArray<float>(numAgents, Allocator.Persistent);

		for (int i = 0; i < numAgents; i++)
		{
			_speedMultiplierOutput[i] = 1f;
			_currentSpeedMultiplier[i] = 1f;
		}

	}

	private void InitializeAgents()
	{
		//instantiate agents
		for (int i = 0; i < numAgents; i++)
		{
			FlockAgentJob newAgent = Instantiate(
					agentPrefab,
					(Vector2)spawnPoint.position + (Random.insideUnitCircle * numAgents * 0.08f),
					Quaternion.Euler(Vector3.forward * Random.Range(0f, 360f)),
					transform
			);
			newAgent.name = agentPrefab.name + i;
			newAgent.Initialize(this);
			newAgent.orbitBias = Random.Range(-1f, 1f);
			_agents.Add(newAgent);
			_agentColliderMap[newAgent.GetComponent<Collider2D>()] = newAgent;
		}
		//cache the transforms for faster access
		agentTransforms = _agents.Select(a => a.transform).ToArray();

		//init behaviors
		foreach (FlockBehaviorBase behavior in _behaviors)
		{
			behavior.Initialize(this);
		}
	}

	private void Update()
	{
		if (!isActive || !_agentsDataArray.IsCreated)
			return;

		if (isCulled)
			return;

		HandleFlockingUpdate();
	}


	private void HandleFlockingUpdate()
	{
		//1. Basic job data
		for (int i = 0; i < numAgents; i++)
		{
			Transform tf = agentTransforms[i];
			_agentsDataArray[i] = new BoidData
			{
				Position = new float2(tf.position.x, tf.position.y),
				Forward = new float2(tf.up.x, tf.up.y)
			};
		}

		//2. Create the Job with Common behavior data
		var job = new FlockDirectionJob
		{
			Agents = _agentsDataArray,
			Output = _steeringOutputArray,
			SpeedMultiplierOutput = _speedMultiplierOutput,
			TimeSeconds = Time.time,
		};

		//3. Calculate behaviors to use
		foreach (FlockBehaviorBase behavior in _behaviors)
		{
			behavior.ApplyToJob(ref job);
		}

		JobHandle handle = job.Schedule(numAgents, 64);
		handle.Complete();

		//apply result back to agents
		for (int i = 0; i < numAgents; i++)
		{
			float2 dir = _steeringOutputArray[i];
			float targetSpeedMultiplier = _speedMultiplierOutput[i];
			float currentMultiplier = _currentSpeedMultiplier[i];
			float smoothingRate = targetSpeedMultiplier > currentMultiplier
					? job.HardAvoidSpeedBoostInRate
					: job.HardAvoidSpeedBoostOutRate;
			currentMultiplier = Mathf.MoveTowards(currentMultiplier, targetSpeedMultiplier, smoothingRate * Time.deltaTime);
			_currentSpeedMultiplier[i] = currentMultiplier;

			float speed = acceleration * currentMultiplier;
			FlockAgentJob agent = _agents[i];

			//apply any external forces from the behaviors, like area containment
			foreach (FlockBehaviorBase behavior in _behaviors)
			{
				dir += behavior.GetExternalDirection(ref agent);
			}

			dir = math.normalize(dir);

			_agents[i].Move(dir * speed);
		}
	}

	private void OnDestroy()
	{
		if (_agentsDataArray.IsCreated) _agentsDataArray.Dispose();
		if (_steeringOutputArray.IsCreated) _steeringOutputArray.Dispose();
		if (_speedMultiplierOutput.IsCreated) _speedMultiplierOutput.Dispose();
		if (_currentSpeedMultiplier.IsCreated) _currentSpeedMultiplier.Dispose();
	}

	//to toggle the whole activity on and off
	public void ToggleActivity(bool newActive)
	{
		this.isActive = newActive;
	}

	public bool ToggleGoal(bool newActive)
	{
		var goal = _behaviors.OfType<FlockGoal>().FirstOrDefault();
		if (goal == null) return false;
		goal.isActive = newActive;
		return true;
	}

	public bool ToggleContainment(bool newActive)
	{
		var containment = _behaviors.OfType<FlockContainment>().FirstOrDefault();
		if (containment == null) return false;
		containment.isActive = newActive;
		return true;
	}

	public bool ToggleGoalType(FlockGoalType newType)
	{
		var goal = _behaviors.OfType<FlockGoal>().FirstOrDefault();
		if (goal == null) return false;
		goal.goalType = newType;
		return true;
	}

	public FlockContainment GetContainmentBehavior()
	{
		return _behaviors.OfType<FlockContainment>().FirstOrDefault();
	}

	#region Culling

	public override Bounds GetCullBounds()
	{
		if (agentTransforms != null && agentTransforms.Length > 0)
		{
			Vector3 startPos = agentTransforms[0].position;
			Bounds bounds = new Bounds(startPos, Vector3.one * 0.5f);
			for (int i = 1; i < agentTransforms.Length; i++)
			{
				bounds.Encapsulate(agentTransforms[i].position);
			}
			return bounds;
		}

		return default;
	}



	public override void SetCulled(bool culled)
	{
		//for the flock manager this only changes a boolean, so it is cheap
		isCulled = culled;
	}

	protected override void OnDrawGizmosSelected()
	{
		//if they are the same it pretty much means it is visible
		if (_cameraClosestPoint == Vector3.one)
			return;

		Bounds c = GetCullBounds();
		Gizmos.color = Color.purple;
		Gizmos.DrawWireCube(c.center, c.size);

		Gizmos.color = isCulled ? Color.red : Color.purple;
		Gizmos.DrawLine(c.center, _cameraClosestPoint);
		Gizmos.DrawSphere(c.center, 1f);
		Gizmos.DrawSphere(_cameraClosestPoint, 1f);

	}

	#endregion


}