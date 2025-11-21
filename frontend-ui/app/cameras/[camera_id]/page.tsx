'use client';

import { useEffect, useState } from 'react';
import { useRouter, useParams } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { ArrowLeft, Play, Square, Edit, RefreshCw, Video, Activity, Settings } from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Camera } from '@/lib/types';

export default function CameraDetailsPage() {
  const router = useRouter();
  const params = useParams();
  const cameraId = params.camera_id as string;

  const [loading, setLoading] = useState(true);
  const [camera, setCamera] = useState<Camera | null>(null);
  const [starting, setStarting] = useState(false);
  const [stopping, setStopping] = useState(false);

  useEffect(() => {
    loadCamera();
    // Refresh camera data every 3 seconds
    const interval = setInterval(loadCamera, 3000);
    return () => clearInterval(interval);
  }, [cameraId]);

  const loadCamera = async () => {
    try {
      const data = await apiClient.getCamera(cameraId);
      setCamera(data);
    } catch (error) {
      console.error('Failed to load camera:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleStart = async () => {
    if (!camera) return;
    setStarting(true);
    try {
      await apiClient.startCamera(camera.camera_id);
      await new Promise(resolve => setTimeout(resolve, 1000));
      await loadCamera();
    } catch (error) {
      console.error('Failed to start camera:', error);
      alert('Failed to start camera');
    } finally {
      setStarting(false);
    }
  };

  const handleStop = async () => {
    if (!camera) return;
    setStopping(true);
    try {
      await apiClient.stopCamera(camera.camera_id);
      await new Promise(resolve => setTimeout(resolve, 1000));
      await loadCamera();
    } catch (error) {
      console.error('Failed to stop camera:', error);
      alert('Failed to stop camera');
    } finally {
      setStopping(false);
    }
  };

  const getStateColor = (state: string) => {
    const stateMap: Record<string, string> = {
      'RUNNING': 'bg-green-500',
      'STOPPED': 'bg-slate-500',
      'STARTING': 'bg-yellow-500',
      'STOPPING': 'bg-orange-500',
      'ERROR': 'bg-red-500',
      'IDLE': 'bg-slate-400',
    };
    return stateMap[state] || 'bg-slate-400';
  };

  if (loading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <RefreshCw className="h-8 w-8 animate-spin text-primary" />
      </div>
    );
  }

  if (!camera) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
        <Header title="Camera Not Found" description="The requested camera could not be found" />
        <main className="container mx-auto px-6 py-8">
          <Link href="/cameras">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Cameras
            </Button>
          </Link>
        </main>
      </div>
    );
  }

  // Build stream URL (if worker supports MJPEG streaming)
  const streamUrl = camera.is_running
    ? `http://localhost:8081/stream/${camera.camera_id}`
    : null;

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header
        title={camera.name}
        description={camera.description || 'Camera details and live feed'}
      />

      <main className="container mx-auto px-6 py-8 max-w-7xl">
        {/* Back Button */}
        <div className="mb-6">
          <Link href="/cameras">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Cameras
            </Button>
          </Link>
        </div>

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Left Column - Video Feed */}
          <div className="lg:col-span-2 space-y-6">
            {/* Live Feed Card */}
            <Card>
              <CardHeader>
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-3">
                    <Video className="h-5 w-5" />
                    <CardTitle>Live Feed</CardTitle>
                    <Badge className={getStateColor(camera.state)}>
                      {camera.state}
                    </Badge>
                  </div>
                  <div className="flex gap-2">
                    {!camera.is_running ? (
                      <Button
                        size="sm"
                        onClick={handleStart}
                        disabled={starting || !camera.enabled}
                      >
                        {starting ? (
                          <><RefreshCw className="h-4 w-4 mr-2 animate-spin" />Starting...</>
                        ) : (
                          <><Play className="h-4 w-4 mr-2" />Start</>
                        )}
                      </Button>
                    ) : (
                      <Button
                        size="sm"
                        variant="destructive"
                        onClick={handleStop}
                        disabled={stopping}
                      >
                        {stopping ? (
                          <><RefreshCw className="h-4 w-4 mr-2 animate-spin" />Stopping...</>
                        ) : (
                          <><Square className="h-4 w-4 mr-2" />Stop</>
                        )}
                      </Button>
                    )}
                  </div>
                </div>
              </CardHeader>
              <CardContent>
                <div className="relative aspect-video bg-slate-900 rounded-lg overflow-hidden">
                  {camera.is_running && streamUrl ? (
                    <img
                      src={streamUrl}
                      alt={camera.name}
                      className="w-full h-full object-contain"
                      onError={(e) => {
                        // Fallback if stream not available
                        e.currentTarget.style.display = 'none';
                      }}
                    />
                  ) : (
                    <div className="absolute inset-0 flex items-center justify-center text-slate-400">
                      <div className="text-center">
                        <Video className="h-16 w-16 mx-auto mb-4 opacity-50" />
                        <p className="text-sm">
                          {camera.enabled
                            ? 'Camera is stopped. Click Start to view live feed.'
                            : 'Camera is disabled. Enable it to start streaming.'}
                        </p>
                      </div>
                    </div>
                  )}
                </div>
                <p className="text-xs text-muted-foreground mt-2">
                  Note: Live streaming requires the camera to be running and worker to support MJPEG/HLS streaming
                </p>
              </CardContent>
            </Card>

            {/* Statistics Card */}
            <Card>
              <CardHeader>
                <div className="flex items-center gap-3">
                  <Activity className="h-5 w-5" />
                  <CardTitle>Statistics</CardTitle>
                </div>
              </CardHeader>
              <CardContent>
                <div className="grid grid-cols-2 gap-4">
                  <div>
                    <p className="text-sm text-muted-foreground mb-1">FPS</p>
                    <p className="text-2xl font-bold">
                      {camera.is_running ? camera.target_fps : '—'}
                    </p>
                  </div>
                  <div>
                    <p className="text-sm text-muted-foreground mb-1">Latency</p>
                    <p className="text-2xl font-bold">
                      {camera.latency_ms}ms
                    </p>
                  </div>
                  <div>
                    <p className="text-sm text-muted-foreground mb-1">Protocol</p>
                    <p className="text-2xl font-bold uppercase">
                      {camera.protocols}
                    </p>
                  </div>
                  <div>
                    <p className="text-sm text-muted-foreground mb-1">Decoder</p>
                    <p className="text-2xl font-bold">
                      {camera.use_nvidia_decoder ? 'GPU' : 'CPU'}
                    </p>
                  </div>
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Right Column - Camera Info */}
          <div className="space-y-6">
            {/* Camera Info Card */}
            <Card>
              <CardHeader>
                <div className="flex items-center justify-between">
                  <CardTitle>Camera Info</CardTitle>
                  <Link href={`/cameras/${camera.camera_id}/edit`}>
                    <Button size="sm" variant="outline">
                      <Edit className="h-4 w-4 mr-2" />
                      Edit
                    </Button>
                  </Link>
                </div>
              </CardHeader>
              <CardContent className="space-y-4">
                <div>
                  <p className="text-sm text-muted-foreground mb-1">Camera ID</p>
                  <p className="font-mono text-sm">{camera.camera_id}</p>
                </div>
                <div>
                  <p className="text-sm text-muted-foreground mb-1">RTSP URL</p>
                  <p className="font-mono text-xs break-all">{camera.rtsp_url}</p>
                </div>
                {camera.username && (
                  <div>
                    <p className="text-sm text-muted-foreground mb-1">Username</p>
                    <p className="font-mono text-sm">{camera.username}</p>
                  </div>
                )}
                <div>
                  <p className="text-sm text-muted-foreground mb-1">Status</p>
                  <div className="flex items-center gap-2">
                    <div className={`h-2 w-2 rounded-full ${getStateColor(camera.state)}`} />
                    <span className="font-semibold">{camera.state}</span>
                  </div>
                </div>
                <div>
                  <p className="text-sm text-muted-foreground mb-1">Enabled</p>
                  <Badge variant={camera.enabled ? 'default' : 'secondary'}>
                    {camera.enabled ? 'Yes' : 'No'}
                  </Badge>
                </div>
              </CardContent>
            </Card>

            {/* Motion Detection Card */}
            {camera.motion_detection_enabled && (
              <Card>
                <CardHeader>
                  <div className="flex items-center gap-3">
                    <Settings className="h-5 w-5" />
                    <CardTitle>Motion Detection</CardTitle>
                  </div>
                </CardHeader>
                <CardContent className="space-y-3">
                  <div className="flex justify-between">
                    <span className="text-sm text-muted-foreground">Status</span>
                    <Badge variant="default">Enabled</Badge>
                  </div>
                  {camera.motion_detection_config && (
                    <>
                      <div className="flex justify-between">
                        <span className="text-sm text-muted-foreground">Algorithm</span>
                        <span className="text-sm font-medium">
                          {camera.motion_detection_config.algorithm}
                        </span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-sm text-muted-foreground">Sensitivity</span>
                        <span className="text-sm font-medium">
                          {camera.motion_detection_config.sensitivity}
                        </span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-sm text-muted-foreground">Min Area</span>
                        <span className="text-sm font-medium">
                          {camera.motion_detection_config.min_contour_area}px
                        </span>
                      </div>
                    </>
                  )}
                </CardContent>
              </Card>
            )}

            {/* Quick Actions Card */}
            <Card>
              <CardHeader>
                <CardTitle>Quick Actions</CardTitle>
              </CardHeader>
              <CardContent className="space-y-2">
                <Link href={`/cameras/${camera.camera_id}/edit`} className="block">
                  <Button variant="outline" className="w-full justify-start">
                    <Edit className="h-4 w-4 mr-2" />
                    Edit Configuration
                  </Button>
                </Link>
                <Link href="/events" className="block">
                  <Button variant="outline" className="w-full justify-start">
                    <Activity className="h-4 w-4 mr-2" />
                    View Events
                  </Button>
                </Link>
              </CardContent>
            </Card>
          </div>
        </div>
      </main>
    </div>
  );
}
