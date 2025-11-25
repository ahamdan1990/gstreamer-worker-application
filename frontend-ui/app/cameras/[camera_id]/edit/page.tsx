'use client';

import { useEffect, useState } from 'react';
import { useRouter, useParams } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { Switch } from '@/components/ui/switch';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { ArrowLeft, Save, Loader2, RefreshCw } from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Camera, UpdateCameraRequest } from '@/lib/types';

export default function EditCameraPage() {
  const router = useRouter();
  const params = useParams();
  const cameraId = params.camera_id as string;

  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [camera, setCamera] = useState<Camera | null>(null);
  const [formData, setFormData] = useState<UpdateCameraRequest>({});

  useEffect(() => {
    loadCamera();
  }, [cameraId]);

  const loadCamera = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getCamera(cameraId);
      setCamera(data);

      // Initialize form with camera data
      setFormData({
        name: data.name,
        description: data.description || '',
        rtsp_url: data.rtsp_url,
        username: data.username || '',
        password: data.password || '',
        protocols: data.protocols,
        latency_ms: data.latency_ms,
        target_fps: data.target_fps,
        enable_display: data.enable_display,
        use_nvidia_decoder: data.use_nvidia_decoder,
        enabled: data.enabled,
        motion_detection: data.motion_detection_config ? {
          ...data.motion_detection_config
        } : {
          enabled: false,
          algorithm: 'MOG2_CUDA',
          sensitivity: 0.7,
          min_contour_area: 500,
          max_contours: 10,
          frame_skip: 1,
          blur_size: 5,
          history: 500,
          var_threshold: 16,
          detect_shadows: true,
          cooldown_seconds: 5,
          required_frames: 2,
          max_frame_width: 0,
          max_frame_height: 0
        }
      });
    } catch (error) {
      console.error('Failed to load camera:', error);
      alert('Failed to load camera');
    } finally {
      setLoading(false);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setSaving(true);

    try {
      // Validate and fix RTSP URL format
      if (formData.rtsp_url) {
        // Ensure forward slashes in RTSP URL
        formData.rtsp_url = formData.rtsp_url.replace(/\\\\/g, '//').replace(/\\/g, '/');

        if (!formData.rtsp_url.startsWith('rtsp://') && !formData.rtsp_url.startsWith('rtsp:/')) {
          alert('RTSP URL must start with rtsp://');
          setSaving(false);
          return;
        }
      }

      await apiClient.updateCamera(cameraId, formData);
      router.push('/cameras');
    } catch (error) {
      console.error('Failed to update camera:', error);
      alert('Failed to update camera. Please check the form and try again.');
    } finally {
      setSaving(false);
    }
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
      <div className="flex items-center justify-center min-h-screen">
        <p>Camera not found</p>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title={`Edit ${camera.name}`} description="Update camera configuration" />

      <main className="container mx-auto px-6 py-8 pb-20 max-w-4xl">
        <div className="mb-6">
          <Link href="/cameras">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Cameras
            </Button>
          </Link>
        </div>

        <form onSubmit={handleSubmit} className="space-y-6">
          {/* Basic Information */}
          <Card>
            <CardHeader>
              <CardTitle>Basic Information</CardTitle>
              <CardDescription>Camera identification and connection details</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="grid grid-cols-2 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="camera_id">Camera ID (read-only)</Label>
                  <Input
                    id="camera_id"
                    value={camera.camera_id}
                    disabled
                    className="bg-slate-100"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="name">Name*</Label>
                  <Input
                    id="name"
                    required
                    value={formData.name || ''}
                    onChange={(e) => setFormData({...formData, name: e.target.value})}
                    placeholder="e.g., Front Door Camera"
                  />
                </div>
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description || ''}
                  onChange={(e) => setFormData({...formData, description: e.target.value})}
                  placeholder="Optional description"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="rtsp_url">RTSP URL*</Label>
                <Input
                  id="rtsp_url"
                  required
                  value={formData.rtsp_url || ''}
                  onChange={(e) => {
                    // Ensure forward slashes
                    const cleanUrl = e.target.value.replace(/\\\\/g, '//').replace(/\\/g, '/');
                    setFormData({...formData, rtsp_url: cleanUrl});
                  }}
                  placeholder="rtsp://192.168.1.100:554/stream"
                />
                <p className="text-xs text-muted-foreground">
                  Must start with rtsp:// (use forward slashes)
                </p>
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="username">Username</Label>
                  <Input
                    id="username"
                    value={formData.username || ''}
                    onChange={(e) => setFormData({...formData, username: e.target.value})}
                    placeholder="admin"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="password">Password</Label>
                  <Input
                    id="password"
                    type="password"
                    value={formData.password || ''}
                    onChange={(e) => setFormData({...formData, password: e.target.value})}
                    placeholder="Leave blank to keep current"
                  />
                </div>
              </div>
            </CardContent>
          </Card>

          {/* Stream Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Stream Configuration</CardTitle>
              <CardDescription>Performance and quality settings</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="grid grid-cols-3 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="protocols">Protocol</Label>
                  <Select
                    value={formData.protocols || 'tcp'}
                    onValueChange={(value) => setFormData({...formData, protocols: value})}
                  >
                    <SelectTrigger id="protocols">
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="tcp">TCP</SelectItem>
                      <SelectItem value="udp">UDP</SelectItem>
                      <SelectItem value="tcp+udp">TCP+UDP</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                <div className="space-y-2">
                  <Label htmlFor="latency_ms">Latency (ms)</Label>
                  <Input
                    id="latency_ms"
                    type="number"
                    value={formData.latency_ms || 150}
                    onChange={(e) => setFormData({...formData, latency_ms: parseInt(e.target.value)})}
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="target_fps">Target FPS</Label>
                  <Input
                    id="target_fps"
                    type="number"
                    value={formData.target_fps || 12}
                    onChange={(e) => setFormData({...formData, target_fps: parseInt(e.target.value)})}
                  />
                </div>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="use_nvidia">NVIDIA Hardware Acceleration</Label>
                  <p className="text-sm text-muted-foreground">Use GPU for video decoding</p>
                </div>
                <Switch
                  id="use_nvidia"
                  checked={formData.use_nvidia_decoder ?? true}
                  onCheckedChange={(checked) => setFormData({...formData, use_nvidia_decoder: checked})}
                />
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="enable_display">Enable Display</Label>
                  <p className="text-sm text-muted-foreground">Show video window (debugging only)</p>
                </div>
                <Switch
                  id="enable_display"
                  checked={formData.enable_display ?? false}
                  onCheckedChange={(checked) => setFormData({...formData, enable_display: checked})}
                />
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="enabled">Camera Enabled</Label>
                  <p className="text-sm text-muted-foreground">Allow this camera to be started</p>
                </div>
                <Switch
                  id="enabled"
                  checked={formData.enabled ?? true}
                  onCheckedChange={(checked) => setFormData({...formData, enabled: checked})}
                />
              </div>
            </CardContent>
          </Card>

          {/* Motion Detection Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Motion Detection Configuration</CardTitle>
              <CardDescription>Configure motion detection settings for this camera</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="motion_enabled">Enable Motion Detection</Label>
                  <p className="text-sm text-muted-foreground">Detect motion in video frames</p>
                </div>
                <Switch
                  id="motion_enabled"
                  checked={formData.motion_detection?.enabled ?? false}
                  onCheckedChange={(checked) => setFormData({
                    ...formData,
                    motion_detection: {
                      ...(formData.motion_detection || {}),
                      enabled: checked
                    }
                  })}
                />
              </div>

              {formData.motion_detection?.enabled && (
                <>
                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="algorithm">Algorithm</Label>
                      <Select
                        value={formData.motion_detection?.algorithm || 'MOG2_CUDA'}
                        onValueChange={(value) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            algorithm: value as 'MOG2_CUDA' | 'MOG2' | 'KNN' | 'FRAME_DIFF'
                          }
                        })}
                      >
                        <SelectTrigger id="algorithm">
                          <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                          <SelectItem value="MOG2_CUDA">MOG2 (GPU)</SelectItem>
                          <SelectItem value="MOG2">MOG2 (CPU)</SelectItem>
                          <SelectItem value="KNN">KNN</SelectItem>
                          <SelectItem value="FRAME_DIFF">Frame Difference</SelectItem>
                        </SelectContent>
                      </Select>
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="sensitivity">Sensitivity (0-1)</Label>
                      <Input
                        id="sensitivity"
                        type="number"
                        step="0.1"
                        min="0"
                        max="1"
                        value={formData.motion_detection?.sensitivity ?? 0.7}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            sensitivity: parseFloat(e.target.value)
                          }
                        })}
                      />
                    </div>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="min_contour_area">Min Contour Area (px)</Label>
                      <Input
                        id="min_contour_area"
                        type="number"
                        value={formData.motion_detection?.min_contour_area ?? 500}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            min_contour_area: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="max_contours">Max Contours</Label>
                      <Input
                        id="max_contours"
                        type="number"
                        value={formData.motion_detection?.max_contours ?? 10}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            max_contours: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>
                  </div>

                  <div className="grid grid-cols-3 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="frame_skip">Frame Skip</Label>
                      <Input
                        id="frame_skip"
                        type="number"
                        value={formData.motion_detection?.frame_skip ?? 1}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            frame_skip: parseInt(e.target.value)
                          }
                        })}
                      />
                      <p className="text-xs text-muted-foreground">Process 1 in N frames</p>
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="blur_size">Blur Size</Label>
                      <Input
                        id="blur_size"
                        type="number"
                        value={formData.motion_detection?.blur_size ?? 5}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            blur_size: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="history">History Frames</Label>
                      <Input
                        id="history"
                        type="number"
                        value={formData.motion_detection?.history ?? 500}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            history: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="var_threshold">Variance Threshold</Label>
                      <Input
                        id="var_threshold"
                        type="number"
                        value={formData.motion_detection?.var_threshold ?? 16}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            var_threshold: parseFloat(e.target.value)
                          }
                        })}
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="cooldown_seconds">Cooldown (seconds)</Label>
                      <Input
                        id="cooldown_seconds"
                        type="number"
                        value={formData.motion_detection?.cooldown_seconds ?? 5}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            cooldown_seconds: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="max_frame_width">Max Frame Width</Label>
                      <Input
                        id="max_frame_width"
                        type="number"
                        placeholder="0 = original"
                        value={formData.motion_detection?.max_frame_width ?? 0}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            max_frame_width: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="max_frame_height">Max Frame Height</Label>
                      <Input
                        id="max_frame_height"
                        type="number"
                        placeholder="0 = original"
                        value={formData.motion_detection?.max_frame_height ?? 0}
                        onChange={(e) => setFormData({
                          ...formData,
                          motion_detection: {
                            ...(formData.motion_detection || {}),
                            max_frame_height: parseInt(e.target.value)
                          }
                        })}
                      />
                    </div>
                  </div>

                  <div className="flex items-center justify-between">
                    <div>
                      <Label htmlFor="detect_shadows">Detect Shadows</Label>
                      <p className="text-sm text-muted-foreground">Use shadow detection (may reduce false positives)</p>
                    </div>
                    <Switch
                      id="detect_shadows"
                      checked={formData.motion_detection?.detect_shadows ?? true}
                      onCheckedChange={(checked) => setFormData({
                        ...formData,
                        motion_detection: {
                          ...(formData.motion_detection || {}),
                          detect_shadows: checked
                        }
                      })}
                    />
                  </div>
                </>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href="/cameras">
              <Button type="button" variant="outline">
                Cancel
              </Button>
            </Link>
            <Button type="submit" disabled={saving}>
              {saving ? (
                <><Loader2 className="h-4 w-4 mr-2 animate-spin" />Saving...</>
              ) : (
                <><Save className="h-4 w-4 mr-2" />Save Changes</>
              )}
            </Button>
          </div>
        </form>
      </main>
    </div>
  );
}
