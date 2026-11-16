public class PredatorManager : SerializedMonoBehaviour
{
	public static PredatorManager Instance { get; private set; }

	// Closest active chaser distance. float.MaxValue when no one is chasing.
	public static float ClosestChaserDistance { get; private set; } = float.MaxValue;
	public static int ActiveCount => Instance._activePredators.Count;
	public static int ChasingCount => Instance._chasingPredators.Count;

	[SerializeField] public List<PredatorPrefabEntry> predatorPrefabs = new List<PredatorPrefabEntry>();
	[SerializeField, Range(1, 10)] private int cullingFrameInterval = 4;

	private HashSet<PredatorBase> _activePredators = new HashSet<PredatorBase>();
	private HashSet<PredatorBase> _chasingPredators = new HashSet<PredatorBase>();
	private Transform _playerTransform;
	private Plane[] _cachedFrustumPlanes;

	private void Awake()
	{
		if (Instance != null && Instance != this)
		{
			Destroy(this);
		}
		else
		{
			Instance = this;
		}
	}

	private void OnEnable()
	{
		PredatorBase.OnStateChanged += OnPredatorStateChanged;
		PredatorBase.OnPredatorDisabled += OnPredatorBecameDisabled;
		PredatorBase.OnPredatorEnabled += OnPredatorBecameEnabled;
		GameManager.OnInitialize += OnGameInitialize;
		GameManager.OnRestartingGame += OnGameRestart;
	}

	private void OnDisable()
	{
		PredatorBase.OnStateChanged -= OnPredatorStateChanged;
		PredatorBase.OnPredatorDisabled -= OnPredatorBecameDisabled;
		PredatorBase.OnPredatorEnabled -= OnPredatorBecameEnabled;
		GameManager.OnInitialize -= OnGameInitialize;
		GameManager.OnRestartingGame -= OnGameRestart;
	}

	private void Update()
	{
		// Updating every other frame is precise enough for this use and halves the cost
		if (Time.frameCount % 2 == 0)
			UpdateClosestChaserDistance();
	}

	private void OnGameInitialize(Transform playerTransform)
	{
		_playerTransform = playerTransform;
	}

	private void OnGameRestart()
	{
		_activePredators.Clear();
		_chasingPredators.Clear();
		ClosestChaserDistance = float.MaxValue;
	}

	private void OnPredatorStateChanged(PredatorBase predator, PredatorState state)
	{
		if (!predator.canAddChaseMusic)
			return;

		if (state == PredatorState.Chasing)
			_chasingPredators.Add(predator);
		else
			_chasingPredators.Remove(predator);
	}

	private void OnPredatorBecameDisabled(PredatorBase predator)
	{
		_activePredators.Remove(predator);
		_chasingPredators.Remove(predator);
	}

	private void OnPredatorBecameEnabled(PredatorBase predator)
	{
		_activePredators.Add(predator);
		if (predator.canAddChaseMusic && predator.CurrentState == PredatorState.Chasing)
			_chasingPredators.Add(predator);
	}



	private void UpdateClosestChaserDistance()
	{
		if (_chasingPredators.Count == 0 || _playerTransform == null)
		{
			ClosestChaserDistance = float.MaxValue;
			return;
		}

		Vector2 playerPos = _playerTransform.position;
		float minSqrDist = float.MaxValue;

		foreach (PredatorBase predator in _chasingPredators)
		{
			if (!predator.gameObject.activeInHierarchy) continue;
			float sqrDist = ((Vector2)predator.transform.position - playerPos).sqrMagnitude;
			if (sqrDist < minSqrDist)
				minSqrDist = sqrDist;
		}

		// Single sqrt at the end, only when we have a valid result
		ClosestChaserDistance = minSqrDist < float.MaxValue ? Mathf.Sqrt(minSqrDist) : float.MaxValue;
	}
}
