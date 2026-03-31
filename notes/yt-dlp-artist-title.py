import yt_dlp

url = "https://www.youtube.com/watch?v=RgKAFK5djSk"
ydl = yt_dlp.YoutubeDL()
with ydl:
	video = ydl.extract_info(url, download=False)
print(video["track"])
print(video["artist"])
