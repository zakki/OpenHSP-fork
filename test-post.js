

        Module.preRun =() => {
          Module["FS_createPath"]("/", "data", true, true);

          ENV.HSP_DPM_PATH = "tamane1.dpm"; //DPMファイル
          ENV.HSP_WX = "$$$scrsize_x$$$";//スクリプトの動作解像度
          ENV.HSP_WY = "$$$scrsize_y$$$";
          ENV.HSP_SX = "$$$dispsize_x$$$";//表示解像度
          ENV.HSP_SY = "$$$dispsize_y$$$";
          ENV.HSP_AUTOSCALE = "0"; //スケーリングモード
          ENV.HSP_FPS = "0"; //フレームレート
          ENV.HSP_LIMIT_STEP = "15000"; //ブラウザに処理を返すまでの実行ステップ数
          //ENV.HSP_SENSOR = String(sensor_button); //センサーを使用する
          console.log('preRun: ENV', ENV);
        };
        
        