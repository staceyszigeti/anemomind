# Full synchronization test procedure

*Description*: This is a more realistic test that we can perform on boxes before delivering them. It requires a bit more labour. For a more regular test, see ```repeatable_test_sync_procedure.md```

## Equipment needed
  * An anemobox that replays data, tagged "Test REPLAY box"
  * An anemobox that we want to test
  * An iOS device with a cable to connect it to a computer
  * A laptop on which to run the server
  * NMEA cables

Note the box id: It is the concatenation of the hexadecimal digits printed on the box.

## Initialization

1. Make sure we have a reasonably clean web server in *DEV* mode running locally on your computer. There might be a user on that web server, but no boat.
2. Compile a clean iOS app with an empty Core Data database. The app should connect to the *DEV* webserver. You can use the ```./prepare_sync_test.sh``` bash script in the anemomind-ios repository, and use ```./prepare_full_reset.sh``` to reset the database of the app.
3. A clean box. You can use the ```/anemonode/factory_reset.sh``` script if the box is not clean.

## Test steps

4. Start the replay box by connecting it to power. The box is tagged "Test REPLAY box".
5. Connect the box that we want to test to the replay box using both NMEA2000 or NMEA0183 or whatever is available.
6. Turn on the app and connect it to the box. You will go through an initialization procedure:
  - Specify on which boat the box is installed
  - Create the test boat with a unique name
  - It should say that it is connected to that boat
  - Go to the settings page and we should see NMEA2000 data under *Boat settings*.
  - On the live view, we should see values being updated.
7. Wait for about 15 minutes.
8. Press the sync button (although we would expect it to happen automatically).
9. Reconnect app to local network in the office.
10. Sync with server.
11. Expect 2 or 3 log files in the directory specified by the server config. In debug mode, it is found under ```www2/uploads/anemologs```. If you did your job correctly cleaning the server, there should only be one directory, prefixed by ```boat``` followed by the **boat id**.
12. Do a ```logcat -s *``` on those files.
  - Check that there is data from all sources we would expect (**TODO**: add a ```--require-source``` option to logcat so that logcat will return 1 (not 0) if a certain source is not present).
13. On the web server, call the script ```www2/synctest/ping_run.sh``` with the *boat id* as an argument (the code in the folder name of the log uploads). 
14. Sync app with server.
15. Sync app with box.
16. Sync app with server.
17. Call ```www2/synctest/ping_check.sh``` to check that the files arrived, it should say ```PASSED :-)```. If it failed, try to sync the app with the box again, and then with the server. It could happen that the response did not go through on the first try, for some reason.

## Clean up

18. Clean up the box by calling ```/anemonode/factory_reset.sh``` on the box.
