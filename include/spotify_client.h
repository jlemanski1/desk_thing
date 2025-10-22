#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class SpotifyClient {
public:
  // Constructor
  SpotifyClient(const char* clientId, const char* clientSecret, const char* refreshToken);

  // Initialization
  bool begin();

  // Playback control
  bool play();
  bool pause();
  bool togglePlayPause();
  bool nextTrack();
  bool previousTrack();
  bool setVolume(int volumePercent);
  bool seekToPosition(int positionMs);

  bool updatePlaybackState();

  // Getters for current state
  bool isPlaying() const { return _isPlaying; }
  int getVolume() const { return _volume; }
  String getTrackName() const { return _trackName; }
  String getArtistName() const { return _artistName; }
  String getAlbumName() const { return _albumName; }
  int getTrackDurationMs() const { return _trackDurationMs; }
  int getTrackProgressMs() const { return _trackProgressMs; }
  String getAlbumArtUrl() const { return _albumArtUrl; }

  // Token management
  bool refreshAccessToken();
  bool isTokenValid() const;

private:
  // Credentials
  const char* _clientId;
  const char* _clientSecret;
  const char* _refreshToken;

  // Token management
  String _accessToken;
  unsigned long _tokenExpiresAt;

  // Current playback state
  bool _isPlaying;
  int _volume;
  String _trackName;
  String _artistName;
  String _albumName;
  int _trackDurationMs;
  int _trackProgressMs;
  String _albumArtUrl;

  // API endpoints
  static const char* TOKEN_URL;
  static const char* PLAYER_URL;
  static const char* PLAY_URL;
  static const char* PAUSE_URL;
  static const char* NEXT_URL;
  static const char* PREVIOUS_URL;

  bool ensureValidToken();
  bool makePlayerRequest(const char* url, const char* method, const char* body = "");
};