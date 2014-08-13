IMDb List Downloader UI
===

Download IMDb lists (either ratings or lists) automatically

1. Login to IMDb and copy all session cookies into a text file as a string
2. Click "Load cookies" and select the cookies
3. Select Ratings or Lists radio button
4. Go to File->Open and select a CSV file with two columns (name of the list on the 
first column and url on the second column). Note: You can't mix ratings lists
and regular lists
5. Set any other preferences and click Download
6. The app asks you where to save and begins to download them

<img src="http://i.imgur.com/4Lo72He.png">

Requirements:

- C++11 compiler
- Qt >= 5.0
