//Dec 2023 - Behavoir script for the player on DeepWorld (USC Games school project)

public class MonsterPlayer : MonoBehaviour
{

    private GameManager gameManager;

    [Header("Movement settings")]
    [SerializeField] private CinemachineVirtualCamera virtualCam;
    [SerializeField] float maxSpeed;
    [SerializeField] float slowSpeed;
    [SerializeField] float rotationSpeed;
    [SerializeField] float swimForce;
    [SerializeField] float timeBetweenSwim;
    [SerializeField] ParticleSystem VFXSwimBubbles;
    [SerializeField] EventReference SFXDeath;
    [SerializeField] EventReference SFXSwim;
    private StudioEventEmitter NormalSwimSFXEmitter;


    Vector2 moveInputValue;
    Vector3 finalMovement;
    Rigidbody2D myBody;

    [HideInInspector]
    public PlayerInput playerInput;



    [Header("Animation")]
    [Tooltip("How much the head scales doing the swim")]
    [Range(1, 2)]
    [SerializeField] float headScaleMax;
    [Range(0.1f, 1)]
    [SerializeField] float headScaleMin;
    [SerializeField] float headShakeDuration = 1;
    [SerializeField] Transform headPart;

    [Header("Audio")]
    [SerializeField] private ParticleSystem VFXVoice;
    [SerializeField] private EventReference SFXCall;

    private float nextSwim;

    // Start is called before the first frame update
    void Start()
    {
        myBody = GetComponent<Rigidbody2D>();
        playerInput = GetComponent<PlayerInput>();
        NormalSwimSFXEmitter = GetComponent<StudioEventEmitter>();
        gameManager = GameObject.FindFirstObjectByType<GameManager>();
    }

 

    /**
     * Function triggeres when theres a movement input. 
     */
    public void InputOnMove(InputAction.CallbackContext context)
    {
        moveInputValue = context.ReadValue<Vector2>();
    }

    public void InputOnSwim(InputAction.CallbackContext context)
    {
        if (context.phase == InputActionPhase.Performed)
        {
            Swimm();
        }
    }

    //Main function that moves the player very very slowly
    public void Move()
    {
        Vector2 dir = moveInputValue.normalized;
        float magnitude = moveInputValue.magnitude;
        //myAnim.SetFloat("Speed", magnitude);
        finalMovement = (dir * slowSpeed * magnitude * Time.fixedDeltaTime);

        myBody.AddForce(finalMovement);

        if(magnitude > 0 )
        {
            if (!NormalSwimSFXEmitter.IsPlaying())
            {
                NormalSwimSFXEmitter.Play();
                Debug.Log("play normal swim");
            }
        }
        else
        {
            if(NormalSwimSFXEmitter.IsPlaying())
            {
                Debug.Log("Stop normal swim");
                NormalSwimSFXEmitter.Stop();
            }
        }
    }

    //Main function that moves the player like a squid. Propulsion by adding force to rigidbody
    void Swimm()
    {
        //swimTimer += Time.deltaTime;
        Vector2 dir = moveInputValue.normalized;
        if (Time.time >= nextSwim)
        {
            //swimm
            float magnitude = moveInputValue.magnitude;
            //myAnim.SetFloat("Speed", magnitude);
            finalMovement = (dir * swimForce * magnitude);

            myBody.AddForce(finalMovement, ForceMode2D.Impulse);
            myBody.velocity = Vector3.ClampMagnitude(myBody.velocity, maxSpeed);
            nextSwim = Time.time + timeBetweenSwim;

            //Head scale animation
            if (magnitude > 0)
            {
                VFXSwimBubbles.Play();
                FMODUnity.RuntimeManager.PlayOneShot(SFXSwim, transform.position);
                Sequence seq = DOTween.Sequence();
                seq.SetEase(Ease.OutCubic);
                seq.Append(headPart.DOScaleY(headScaleMax, headShakeDuration));
                seq.Append(headPart.DOScaleY(1f, headShakeDuration  * 1.5f));

            }
            //metric handler
            MetricManagerScript.instance?.LogString("Player Action", "Swim");
        }

    }

    private void FixedUpdate()
    {
        Move();
        SlowTurn();
    }

    //Handles the ability for the player to slowly turn, making it more realistic
    private void SlowTurn()
    {
        if (moveInputValue.magnitude > 0f)
        {

            Vector3 rotatedTemp = Quaternion.Euler(0, 0, 0) * new Vector3(moveInputValue.x, moveInputValue.y, 0f);

            Quaternion tempRotation = Quaternion.LookRotation(Vector3.forward, rotatedTemp);
            this.transform.rotation = Quaternion.RotateTowards(transform.rotation, tempRotation, rotationSpeed * Time.fixedDeltaTime);
        }
    }


    private void OnCollisionEnter2D(Collision2D collision)
    {
        if(collision.gameObject.CompareTag("Enemy") && !gameManager.isPlayerDead)
        {
            BeforeGameOver();
        }
    }

    private void BeforeGameOver()
    {
        gameManager.isPlayerDead = true;
        FMODUnity.RuntimeManager.PlayOneShot(SFXDeath, transform.position);
        
        playerInput.DeactivateInput();
        gameObject.SetActive(false);
        gameManager.GameOver();
    }

    public IEnumerator PlayCallSFX()
    {
        VFXVoice.Play();
        yield return new WaitForSeconds(0.3f);
        FMODUnity.RuntimeManager.PlayOneShot(SFXCall, transform.position);
    }

}

