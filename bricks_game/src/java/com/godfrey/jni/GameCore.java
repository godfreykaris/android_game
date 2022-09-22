
package com.godfrey.jni;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Toast;
import android.view.MotionEvent;

import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

import android.view.View;
import android.view.View.OnTouchListener;

import android.widget.AdapterView;
import android.widget.ArrayAdapter;

import android.util.Log;

import android.content.res.AssetManager;

import android.widget.TextView;
import android.widget.ListView;
import android.widget.Switch;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;

import android.content.Intent;
import android.content.IntentFilter;

import android.widget.AdapterView;
import android.widget.ArrayAdapter;

import android.content.Context;

import java.util.Set;

import java.lang.reflect.Method;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class GameCore extends Activity implements SurfaceHolder.Callback
{

    private static String TAG = "GAME";
	
	//application views
	static TextView scoreview;
	static Switch pause;
	static ListView pairedDevicesList;
	
	//applicaton pause state
	Boolean pauseState = false;
		
	//bluetooth connection
	Boolean two_players = true;
	BluetoothAdapter bluetoothAdapter;
	BroadcastReceiver receiver;
	DataOutputStream outputStream;
	
	Toast bluetoothOn;
	Toast bluetoothconnected;
	Toast bluetoothdisconnected;
	
	Boolean CONNECTED = false;
	Boolean DISCONNECTED = true;
	Boolean DISCONNECT_REQUESTED = false;
	
	int connection_try = 1;
	Boolean connectionset = true;
	
	//for qierying the state of the connection
	IntentFilter filter;
		
	boolean bluetoothThreadisRunning = true;
	
	int k = 1;
	
    @Override
    public void onCreate(Bundle savedInstanceState) 
	{
        super.onCreate(savedInstanceState);

        Log.i(TAG, "onCreate()");
        
        setContentView(R.layout.activity_main);
        SurfaceView surfaceView = (SurfaceView)findViewById(R.id.surfaceview);
        surfaceView.getHolder().addCallback(this);
        surfaceView.setOnClickListener(new View.OnClickListener() 
		{
                public void onClick(View view) {
                    Toast toast = Toast.makeText(GameCore.this,
                                                 "This demo combines Java UI and native EGL + OpenGL renderer",
                                                 Toast.LENGTH_LONG);
                    //toast.show();
                }});
				
        surfaceView.setOnTouchListener(new View.OnTouchListener() 
		{
			@Override
			public boolean onTouch(View view, MotionEvent event)
			   {
				  int x = (int) event.getX();
				  int y = (int) event.getY();

                  switch (event.getAction()) 
				  {
                       case MotionEvent.ACTION_DOWN:
					   {
						   // send the touch coordinates to the native code for processing
						   nativeOnInput(x,y);
						   break;
					   }                       
				  }
				  return false;
                }
		});
			 
				bluetoothOn = Toast.makeText(GameCore.this,"Bluetooth is Set", Toast.LENGTH_SHORT);
				bluetoothconnected = Toast.makeText(GameCore.this,"Device is connected", Toast.LENGTH_SHORT);
				bluetoothdisconnected = Toast.makeText(GameCore.this,"Device is disconnected", Toast.LENGTH_SHORT);
				
				// TextView for the scores
				scoreview = (TextView)findViewById(R.id.score);
				// Switch for pausing the game
				pause = (Switch)findViewById(R.id.pause);
								
				//get the Bluetooth adapter that the phone is using
				bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
				
				if(two_players)
				{
					filter = new IntentFilter();
					filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
					filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECT_REQUESTED);
					filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
					this.registerReceiver(broadcastReceiver, filter);
				}
				
				
  }

    @Override
    protected void onStart() 
	{
        super.onStart();
        Log.i(TAG, "onStart()");
				
		// if the game is to be played by two players, launch the bluetooth thread			
				if(two_players)
				{					
					bluetoothCoreThread();
				}
				
		//create the renderer thread in the native code		
        nativeOnStart();
		
		
		// pass the asset manager to native code to allow access to game models there
		nativeSetAssetManager(this.getAssets());
		
		// launch the communication channel from native code to java
		nativeToJavaChannelCoreThread();
	
    }

    @Override
    protected void onResume() 
	{
        super.onResume();
        Log.i(TAG, "onResume()");
        nativeOnResume();
    }
    
    @Override
    protected void onPause() 
	{
        super.onPause();
        Log.i(TAG, "onPause()");
        nativeOnPause();
    }

    @Override
    protected void onStop() 
	{
        super.onStop();
        Log.i(TAG, "onStop()");
        nativeOnStop();
    }
	
	@Override
	protected void onDestroy() 
	{
		super.onDestroy();
       // unregister the ACTION_FOUND receiver.
		unregisterReceiver(receiver);
	}

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) 
	{
        nativeSetSurface(holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) 
	{
    }

    public void surfaceDestroyed(SurfaceHolder holder) 
	{
        nativeSetSurface(null);
    }

	static public boolean setScore(int n)
	{
		scoreview.setText(String.valueOf(n));
		return true;
	}
	
	public void nativeToJavaChannelCoreThread()
	{
		// this thread provides a runtime channel from native code to java
		new Thread() 
			{
				public void run() 
				{
					//This loop should run untill we stop the game as it serves as a channel from native code to java
					while(true)
					{						
						try
						{
							// we it to run in the UI thread so as to be able to access the application views
							runOnUiThread(new Runnable() {
								@Override
								public void run() 
								{
									// get the scores from the native code and call setScore to update the score TextView
									nativeScoreUpdater();
									// have the user turned on the pause switch
									pauseState = pause.isChecked();
									// if pause switch is on, pause the game from the native code of the game
									nativePause(pauseState);
																			
								}
                        });
							//make this thread to sleep for a second so that it passes control and resources to other threads
							Thread.sleep(1000);
						}
						catch(Exception e)
						{
							Log.i(TAG, "Exception thrown in the native to java channel thread.");
						}
						
					}
					
				}        
			}.start();
	}
	
	public void bluetoothCoreThread()
	{
		
		new Thread() 
			{
				public void run() 
				{
					//the loop runs until the bluetooth thread is stopped
					while(bluetoothThreadisRunning)
					{						
						try
						{
							runOnUiThread(new Runnable() {
								@Override
								public void run() 
								{
									boolean enabled = bluetoothAdapter.isEnabled();
									if(enabled)
										{
											// show the bluetooth on toast
											bluetoothOn.show();
											
											if(CONNECTED == true)
											{
												bluetoothconnected.show();
											}
											else
											{
												bluetoothdisconnected.show();
											}
											
											// Initialize array adapters. One for already paired devices and
											// one for newly discovered devices
											ArrayAdapter<String> pairedDevicesArrayAdapter = new ArrayAdapter<String>(GameCore.this, android.R.layout.simple_list_item_1);
        
											// Find and set up the ListView for paired devices
											pairedDevicesList = findViewById(R.id.paired_devices);
											pairedDevicesList.setAdapter(pairedDevicesArrayAdapter);
											pairedDevicesList.setOnItemClickListener(mDeviceClickListener);
				
											//get a set of paired devices
											Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
					
											//if there is a paired device get the name and MAC address of each device
											if (pairedDevices.size() > 0) 
												{
													// There are paired devices. Get the name and address of each paired device.
													for (BluetoothDevice device : pairedDevices) 
														{
															String deviceName = device.getName();
															String deviceHardwareAddress = device.getAddress(); // MAC address
															pairedDevicesArrayAdapter.add(deviceName + "\n" + deviceHardwareAddress);
														}
												}
												else
												{
													String noDevices = "No paired Devices";
													pairedDevicesArrayAdapter.add(noDevices);
												}
										}
																												
								}
                        });
							//make this thread to sleep for a 3 seconds so that it passes control and resources to other threads
							Thread.sleep(6000);
						}
						catch(Exception e)
						{
							Log.i(TAG, "Exception thrown in the bluetooth thread.");
						}
						
					}
					
				}        
			}.start();		
		
	}
	
	// This receiver will only be registered when two players are to play via bluetooth
	public final BroadcastReceiver broadcastReceiver = new BroadcastReceiver() 
	{
      BluetoothDevice device;
	  
      @Override
      public void onReceive(Context context, Intent intent) 
	  {
         String action = intent.getAction();
         device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
		 
         if (BluetoothDevice.ACTION_ACL_CONNECTED.equals(action)) 
		 {
            CONNECTED = true;
			if(connection_try == 3)
			{
				pairedDevicesList.setVisibility(View.INVISIBLE);
			}
				
			connection_try++;
         } 
		 else if (BluetoothDevice.ACTION_ACL_DISCONNECTED.equals(action)) 
		 {
            CONNECTED = false;
         }
      }
   };
   
   private AdapterView.OnItemClickListener mDeviceClickListener = new AdapterView.OnItemClickListener() 
   {
        public void onItemClick(AdapterView<?> av, View v, int arg2, long arg3) 
		{
            // Cancel discovery because it's costly and we're about to connect
            bluetoothAdapter.cancelDiscovery();

            // Get the device MAC address, which is the last 17 chars in the View
            String info = ((TextView) v).getText().toString();
            String address = info.substring(info.length() - 17);
			
			Toast bth = Toast.makeText(GameCore.this,info, Toast.LENGTH_SHORT);
						
			pairedDevicesList.setVisibility(View.INVISIBLE);
			
			bth.show();
			
			try
			{
				BluetoothDevice device = bluetoothAdapter.getRemoteDevice(address);

				Method m = device.getClass().getMethod("createRfcommSocket", new Class[] {int.class});

				BluetoothSocket clientSocket =  (BluetoothSocket) m.invoke(device, 1);

				clientSocket.connect();

				outputStream = new DataOutputStream(clientSocket.getOutputStream());

				new clientSock().start();
				
			} 
			catch (Exception e) 
			{
				e.printStackTrace();
				Log.e("BLUETOOTH", e.getMessage());
			}
    
            /*// Create the result Intent and include the MAC address
            Intent intent = new Intent();
            intent.putExtra(EXTRA_DEVICE_ADDRESS, address);

            // Set result and finish this Activity
            setResult(Activity.RESULT_OK, intent);
            finish();*/
        }
    };
	
	public class clientSock extends Thread {
    public void run () {
        try {
            outputStream.writeBytes("this is a test"); // anything you want
            outputStream.flush();
        } catch (Exception e1) {
            e1.printStackTrace();
            return;
        }
    }
}
	
    public static native void nativeOnStart();
    public static native void nativeOnResume();
    public static native void nativeOnPause();
    public static native void nativeOnStop();
    public static native void nativeSetSurface(Surface surface);
	public static native void nativeSetAssetManager(AssetManager assetmanager);
	public static native void nativeOnInput(int x, int y);
	public static native void nativeScoreUpdater();
	public static native void nativePause(boolean pausestate);
	
	

    static 
	{
        System.loadLibrary("nativecodelib");
    }

}
