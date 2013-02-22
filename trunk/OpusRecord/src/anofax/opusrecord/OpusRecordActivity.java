package anofax.opusrecord;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import android.app.Activity;
import android.app.AlertDialog;
import android.media.MediaScannerConnection;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.Spinner;
import android.widget.TextView;

public class OpusRecordActivity extends Activity {
	boolean _isRunning = false;	
	String _fname = null;
	Button _b;
	
	//sampling rate, in Hz, of the recording
	private int samplingRate;
	
	/*handles to elements in the ui*/
	//sampling rate container
	private RelativeLayout samplingRateLayout;	
	//volume display container
	private RelativeLayout volumeDisplayLayout;
	//text view for displaying volume
	private TextView volumeDisplayTextView;	
	//pointer on volume meter
	private ImageView volumeMeterPointer;	
	
	private DecimalFormat df = new DecimalFormat("#.####");
	
	//UI mode "init" (recording not running)
	private static final int UI_INIT = 0;
	//UI mode "running" (recording running)
	private static final int UI_RUNNING = 1;
	
	class EncThread extends Thread {
		@Override		
		public void run(){
			Native.encode(_fname, samplingRate);
			runOnUiThread( new Runnable(){
				public void run(){
				
				 //update UI: recording not running
				 setUiMode(UI_INIT);
					
				_b.setText("Start Encoder");
				_isRunning = false;
			}});
		}
	}
	EncThread _encoder = null;
	
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_opus_record);
		
		/*handles to elements in the ui*/
		//sampling rate container
		samplingRateLayout = (RelativeLayout) findViewById(R.id.samplingRateLayout);
		//volume display container
		volumeDisplayLayout = (RelativeLayout) findViewById(R.id.volumeDisplayLayout);
		//text view for displaying volume
		volumeDisplayTextView = (TextView) findViewById(R.id.volumeDisplayTextView);
		//pointer on volume meter
		volumeMeterPointer = (ImageView) findViewById(R.id.volumeMeterPointer);
	}
	
	void doStop(){

		//update UI: recording not running
		setUiMode(UI_INIT);
		
		Native.stopRecorder();
		try {
			_encoder.join();
		} catch (InterruptedException e) {}
		_encoder = null;
		MediaScannerConnection.scanFile(this, new String[]{_fname}, null, null );				
	}
	
	@Override	
	protected void onDestroy(){
		super.onDestroy();
		if( _isRunning ){
			doStop();
		}
		
	} 
	
	void showError(String text){
		AlertDialog.Builder alertDialog = new AlertDialog.Builder(this);
		alertDialog.setTitle("Error");
		alertDialog.setMessage(text);
		alertDialog.setPositiveButton("OK", null);
		alertDialog.show();
		
	}

	boolean createFileName() {		
		File path = Environment.getExternalStorageDirectory ();
		try {
			String s = path.getCanonicalPath() + "/OpusRecord/";
			_fname = s+new SimpleDateFormat("yyyy-MM-dd_HH.mm.ss", Locale.US).format(new Date())+".opus";
			new File(s).mkdirs();
			Log.i("URI", _fname);
			new FileWriter(_fname).close();
			return true;
		} catch (IOException e) {
			showError("Could not Open File");
		}
		return false;
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
//		getMenuInflater().inflate(R.menu.activity_opus_record, menu);
		return true;
	}

	void volumeThread(){
		new Thread(){ public void run(){	

			//get scale for volume display
			ImageView volumeMeter = (ImageView) findViewById(R.id.volumeMeter);
			final float volumeMeterWidth = volumeMeter.getDrawable().getIntrinsicWidth();
				while(true){
					final float v = Native.volume();
					Log.i("volume", ""+v);					
					if( v==289) break;
					//update volume display
					runOnUiThread( new Runnable(){
						public void run(){
						volumeDisplayTextView.setText("Volume: "+df.format(v));
						//update volume meter
						volumeMeterPointer.setPadding(((int) (v * 0.5f * volumeMeterWidth)),0,0,0);
					}});
				}	
		}}.start();
	}
	
	public void buttonClicked( View v ){
		Button b = (Button) v;
		_b = b;

		if( _isRunning ){
			doStop();

		} else {

			//get sampling rate selected from dropdown box
			Spinner samplingRateSpinner = (Spinner) findViewById(R.id.samplingRateSpinner);
			String samplingRateText = samplingRateSpinner.getSelectedItem().toString();
			samplingRate =  Integer.parseInt(samplingRateText);
			
			Log.d("audio", "going to start recording, sampling rate is " + samplingRate);
			
			//update UI: recording running
			setUiMode(UI_RUNNING);
			
			if( !createFileName())
				return;
			Native.startRecorder( samplingRate );
			_encoder = new EncThread();
			_encoder.start();
			b.setText( "Stop Recording");
			_isRunning=true;
			volumeThread();
			
		}
	}
	
    /**
     * Updates the UI depending on whether the recorder is running or not.
     * @param int mode The mode, UI_RUNNING if the recorder is running or UI_VIEW if it's not
     */
	private void setUiMode(int mode)
	{
		if (mode == UI_RUNNING)
		{
			//update UI for "running" mode
			//hide sampling rate dropdown box
			samplingRateLayout.setVisibility(View.GONE);
			//show volume display field
			volumeDisplayLayout.setVisibility(View.VISIBLE);
		} else
		{
			//update UI for "init" mode
			//show sampling rate dropdown box
			samplingRateLayout.setVisibility(View.VISIBLE);
			//hide volume display field
			volumeDisplayLayout.setVisibility(View.GONE);
		}
		
	}

}
