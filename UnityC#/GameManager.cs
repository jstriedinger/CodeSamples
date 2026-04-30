using System;
using System.Collections;
using BehaviorDesigner.Runtime;
using UnityEngine;
using DG.Tweening;
using UnityEngine.SceneManagement;
using FMODUnity;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
using UnityEngine.UI;
using UnityEngine.InputSystem;
using UnityEngine.Serialization;

public enum GameState
{
    Default,
    Paused,
    MainMenu,
    Cinematic
}


public class GameManager : MonoBehaviour
{
    public static GameManager Instance { get; private set; }

    public const string PlayerTag = "Player";
    public const string BlueNpcTag = "Blue";
    public const string PredatorLayerAndTag = "Predator";
    public static Transform PlayerTransform { get; private set; }
    public static event Action<Transform> OnInitialize;
    public static event Action OnGameOverStarts;
    public static event Action<Transform,Transform> OnGameOverFinish;
    public static event Action OnRestartingGame, OnStartGame;

    [Header("Global")] 
    private InputAction _anyKeyAction;
    public PlayerCharacter playerRef;
    public GameObject playerLastPosition;
    public BlueNPC blueNpcRef;
    
    [HideInInspector]
    //public bool isPlayerDead;
   
    private GameState _gameState;
    private Vector3 _originPos;
    
    private void OnEnable()
    {
        PlayerCharacter.OnPauseGame += TogglePauseGameGame;
        PlayerCharacter.OnPlayerGotEaten += HandleGameoverStarts;
        LevelManager.OnCheckpointStart += StartGameOnCheckpoint;
    }

    private void OnDisable()
    {
        PlayerCharacter.OnPauseGame -= TogglePauseGameGame;
        LevelManager.OnCheckpointStart -= StartGameOnCheckpoint;
        PlayerCharacter.OnPlayerGotEaten -= HandleGameoverStarts;
        DOTween.KillAll();
    }


    private void Awake() 
    { 
        // If there is an instance, and it's not me, delete myself.
        if (Instance != null && Instance != this) 
        { 
            Destroy(this); 
        } 
        else 
        { 
            Instance = this; 
        } 
    }
    
    void Start()
    {
        //framerate
        //QualitySettings.vSyncCount = 0;
        Application.targetFrameRate = 60;
        OnInitialization();
        
    }
    
    //Start game UI button
    public void StartGame()
    {
        OnStartGame?.Invoke();
    }
    

    //initial Gamemanager and tells other to do so as well
    public void OnInitialization()
    {
        PlayerTransform = playerRef.transform;
        _originPos = PlayerTransform.position;
        _gameState = GameState.Cinematic;
        playerLastPosition.transform.position = _originPos;
        GlobalVariables.Instance.SetVariableValue("playerRef", playerRef.gameObject);
        OnInitialize?.Invoke(PlayerTransform);
    }

    //Prepares everything needed if you are starting from a checkpoint
    public void StartGameOnCheckpoint(CheckPoint cp)
    {
        int checkpointIndex = cp.index;
        //not default then is a checkpint, therefore change game state to default
        if ( checkpointIndex > 0 )
        {
            ChangeGameState(GameState.Default);
            //Can detect monsters and follow with eyes! 
            if (checkpointIndex >= 3 )
            {
                playerRef.ToggleMonsterEyeDetection(true);
                playerRef.ToggleEyeFollowTarget(true);
            }
        }
        
		//Prepare everything to start from a checkpoint or something
        switch (checkpointIndex)
        {
            case 0: //default
                //playing the way it is supposed to be played
                ChangeGameState(GameState.Cinematic);
                BeforeStartTitles();
                break;
            case 1:
                //temp for testing fishbowl
                playerRef.SetBlueReference(blueNpcRef);
                playerRef.ToggleEyeFollowTarget(true,blueNpcRef.transform);
                blueNpcRef.ToggleEyeFollowTarget(true,playerRef.transform);
                CameraManager.Instance.UpdatePlayerRadius(4);
                CameraManager.Instance.AddObjectToCameraView(GameManager.Instance.blueNpcRef.transform, false, CameraManager.Instance.camZoomPlayer, 1);
                blueNpcRef.ToggleFollow(true);
                blueNpcRef.ToggleReactToCall(true);
                break;
            case 6: //final chase
                playerRef.SetBlueReference(blueNpcRef);
                blueNpcRef.ToggleFollow(true);
                blueNpcRef.GetHurt();
                blueNpcRef.ChangeBlueStats(playerRef.transform);
                break;
            
        }
        playerRef.transform.position = cp.GetSpawnPoint().position;
        Transform blueSpawn = cp.GetBlueSpawnPoint();
        if (blueSpawn)
            blueNpcRef.transform.position = blueSpawn.position;
        
    }
    
    //Setup evrything to wait for player input
    public void BeforeStartTitles()
    {
        playerRef.StopMovement();
        playerRef.swimStage = false;
        blueNpcRef.ToggleFollow(false);
        CameraManager.Instance?.ToggleDefaultNoise(false);
        CameraManager.Instance?.ChangeCameraTrackingTarget(CinematicsManager.Instance?.logosFollowCamObj);
        if (UIManager.Instance)
        {
            UIManager.Instance.startToContinue.DOFade(1, 1).SetEase(Ease.InCubic).SetDelay(2)
                .OnComplete(() =>
                {
                    _anyKeyAction = new InputAction(binding: "*/<Button>");
                    _anyKeyAction.performed += OnStartingTitles;
                    _anyKeyAction.Enable();
                });
            
        }
    }

    public void OnStartingTitles(InputAction.CallbackContext ctx)
    {
        _anyKeyAction.Disable();
        //_anyKeyAction.Dispose();
        UIManager.Instance.startToContinue.DOFade(0, 1).OnComplete(() => { Destroy(UIManager.Instance.startToContinue.gameObject); });
        CinematicsManager.Instance.DoCinematicTitles();
    }
    
    

    /**
     * Changes the game state and takes care of anything that must be done when the game enters that state
     */
    public void ChangeGameState(GameState pNewState)
    {
        if (pNewState != _gameState)
        {
            switch(pNewState) 
            { 
                case GameState.MainMenu:
                    playerRef.ToggleInputMap(true);
                    playerRef.ToggleInput(true);
                    break;
                case GameState.Cinematic:
                    playerRef.ToggleInput(false);
                    break;
                case GameState.Paused:
                    playerRef.ToggleInputMap(true);
                    AudioManager.Instance.TogglePauseAudio(true);
                    UIManager.Instance.PauseGame(true);
                    break;
                case GameState.Default:
                    AudioManager.Instance.TogglePauseAudio(false);
                    if (_gameState == GameState.Paused)
                    {
                        UIManager.Instance.PauseGame(false);
                    }
                    
                    //always come back to playerCharacter action map
                    playerRef.ToggleInputMap(false);
                    Time.timeScale = 1;
                    break;
                default:
                    Gamepad.current?.ResumeHaptics();
                    break;

            }
            _gameState = pNewState;
            
        }

    }

    //When playerCharacter gets eaten
    public void HandleGameoverStarts(GameObject monster)
    {
        StartCoroutine(OnGameoverStarted(monster));
        
    }

    IEnumerator OnGameoverStarted(GameObject monster)
    {
        ChangeGameState(GameState.Cinematic);
        OnGameOverStarts?.Invoke();
        yield return new WaitForSeconds(3.1f);
        OnGameOverFinish?.Invoke(playerRef.transform, blueNpcRef.transform);
        yield return new WaitForSeconds(1f);
        OnRestartingGame?.Invoke();
        ChangeGameState(GameState.Default);
        //MetricManagerScript.instance?.LogString("Death", "1");
    }

    #region UI
    public void TogglePauseGameGame()
    {
        if (!UIManager.Instance.isPauseFading)
        {
            //only pause if we are not in a cinematic
            if (_gameState != GameState.Cinematic && _gameState != GameState.MainMenu)
            {
                if (_gameState == GameState.Paused)
                    ChangeGameState(GameState.Default);
                else
                    ChangeGameState(GameState.Paused);
            }
        }
    }
    
    public void QuitGame()
    {
        #if UNITY_EDITOR
                UnityEditor.EditorApplication.isPlaying = false;
        #else
            Application.Quit();
        #endif
    }

    public void ShowMainMenuFirstTime()
    {
        Debug.Log("Show main menu");
        ChangeGameState(GameState.MainMenu);
        UIManager.Instance.ShowMainMenu();
        UIManager.Instance?.SelectStartButtonMainMenu();
    }

    public void UIShowCredits()
    {
        UIManager.Instance.ShowMenuCredits();
    }

    public void UIShowMenu()
    {
        UIManager.Instance.ShowMainMenu();
    }
    #endregion

    private void OnApplicationQuit()
    {
        Gamepad.current?.PauseHaptics();
    }

    private void OnApplicationPause(bool pauseStatus)
    {
        if(pauseStatus)
            Gamepad.current?.PauseHaptics();
        else
            Gamepad.current?.ResetHaptics();
    }

    private void OnApplicationFocus(bool hasFocus)
    {
        if(!hasFocus)
            Gamepad.current?.PauseHaptics();
        else
            Gamepad.current?.ResetHaptics();
    }

    public void TempTeleportToFinal()
    {
        LevelManager.Instance?.ToggleLevelSection(4,false);
        LevelManager.Instance?.ToggleLevelSection(5,true);
        playerRef.SetBlueReference(blueNpcRef);
        blueNpcRef.gameObject.SetActive(true);
        blueNpcRef.ToggleFollow(true);
        blueNpcRef.GetHurt();
        blueNpcRef.ChangeBlueStats(playerRef.transform);

        CheckPoint cp = LevelManager.Instance?.checkPoints[6];
        Transform sp = cp.GetSpawnPoint();
        playerRef.transform.position = cp.GetSpawnPoint().position;
        Transform blueSpawn = cp.GetBlueSpawnPoint();
        if (blueSpawn)
            blueNpcRef.transform.position = blueSpawn.position;
    }


}

