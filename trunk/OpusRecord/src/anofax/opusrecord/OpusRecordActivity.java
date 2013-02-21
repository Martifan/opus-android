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
import android.widget.RelativeLayout;
import android.widget.Spinner;
import android.widget.TextView;

public class OpusRecordActivity extends Activity {
	boolean _isRunning = false;	
	String _fname = null;
	Button _b;
	
	//sampling rate, in Hz, of the recording
	private int samplingRate;
	
	//text view for displaying volume
	private TextView volumeDisplayTextView;
	
	private DecimalFormat df = new DecimalFormat("#.####");
	
	class EncThread extends Thread {
		@Override		
		public void run(){
			Native.encode(_fname, samplingRate);
			runOnUiThread( new Runnable(){
				public void run(){
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
	}
	
	void doStop(){
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
				while(true){
					final float v = Native.volume();
					Log.i("volume", ""+v);					
					if( v==289) break;
					//update volume display
					runOnUiThread( new Runnable(){
						public void run(){
						volumeDisplayTextView.setText("Volume: "+df.format(v));
					}});
				}	
		}}.start();
	}
	
	public void buttonClicked( View v ){
		Button b = (Button) v;
		_b = b;
		
		//sampling rate container
		RelativeLayout samplingRateLayout = (RelativeLayout) findViewById(R.id.samplingRateLayout);	
		//volume display field
		volumeDisplayTextView = (TextView) findViewById(R.id.volumeDisplayTextView);
		
		if( _isRunning ){
			doStop();
			
			//show sampling rate dropdown box
			samplingRateLayout.setVisibility(View.VISIBLE);
			//hide volume display field
			volumeDisplayTextView.setVisibility(View.GONE);
			
		} else {

			//get sampling rate selected from dropdown box
			Spinner samplingRateSpinner = (Spinner) findViewById(R.id.samplingRateSpinner);
			String samplingRateText = samplingRateSpinner.getSelectedItem().toString();
			samplingRate =  Integer.parseInt(samplingRateText);
			
			Log.d("audio", "going to start recording, sampling rate is " + samplingRate);
			
			//hide sampling rate dropdown box
			samplingRateLayout.setVisibility(View.GONE);
			//show volume display field
			volumeDisplayTextView.setVisibility(View.VISIBLE);
			
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

}
