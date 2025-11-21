'use client';

import { useState } from 'react';
import { useRouter } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { Switch } from '@/components/ui/switch';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { ArrowLeft, Save, Loader2 } from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { CreateCameraRequest } from '@/lib/types';

export default function AddCameraPage() {
  const router = useRouter();
  const [loading, setLoading] = useState(false);
  const [formData, setFormData] = useState<CreateCameraRequest>({
    camera_id: '',
    name: '',
    description: '',
    rtsp_url: '',
    username: '',
    password: '',
    protocols: 'tcp',
    latency_ms: 150,
    target_fps: 12,
    enable_display: false,
    use_nvidia_decoder: true,
    motion_detection: {
      enabled: false,
      algorithm: 'MOG2_CUDA',
      sensitivity: 0.5,
      min_contour_area: 500,
      max_contours: 50,
      frame_skip: 2,
      blur_size: 21,
      history: 500,
      var_threshold: 16.0,
      detect_shadows: false,
      cooldown_seconds: 1.0,
      required_frames: 3,
      max_frame_width: 640,
      max_frame_height: 480
    }
  });

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);

    try {
      // Validate and fix RTSP URL format
      if (formData.rtsp_url) {
        // Ensure forward slashes in RTSP URL
        formData.rtsp_url = formData.rtsp_url.replace(/\\\\/g, '//').replace(/\\/g, '/');

        if (!formData.rtsp_url.startsWith('rtsp://') && !formData.rtsp_url.startsWith('rtsp:/')) {
          alert('RTSP URL must start with rtsp://');
          setLoading(false);
          return;
        }
      }

      await apiClient.createCamera(formData);
      router.push('/cameras');
    } catch (error) {
      console.error('Failed to create camera:', error);
      alert('Failed to create camera. Please check the form and try again.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Add Camera" description="Configure a new camera" />

      <main className="container mx-auto px-6 py-8 max-w-4xl">
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
                  <Label htmlFor="camera_id">Camera ID*</Label>
                  <Input
                    id="camera_id"
                    required
                    value={formData.camera_id}
                    onChange={(e) => setFormData({...formData, camera_id: e.target.value})}
                    placeholder="e.g., front_door"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="name">Name*</Label>
                  <Input
                    id="name"
                    required
                    value={formData.name}
                    onChange={(e) => setFormData({...formData, name: e.target.value})}
                    placeholder="e.g., Front Door Camera"
                  />
                </div>
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description}
                  onChange={(e) => setFormData({...formData, description: e.target.value})}
                  placeholder="Optional description"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="rtsp_url">RTSP URL*</Label>
                <Input
                  id="rtsp_url"
                  required
                  value={formData.rtsp_url}
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
                    value={formData.username}
                    onChange={(e) => setFormData({...formData, username: e.target.value})}
                    placeholder="admin"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="password">Password</Label>
                  <Input
                    id="password"
                    type="password"
                    value={formData.password}
                    onChange={(e) => setFormData({...formData, password: e.target.value})}
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
                    value={formData.protocols}
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
                    value={formData.latency_ms}
                    onChange={(e) => setFormData({...formData, latency_ms: parseInt(e.target.value)})}
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="target_fps">Target FPS</Label>
                  <Input
                    id="target_fps"
                    type="number"
                    value={formData.target_fps}
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
                  checked={formData.use_nvidia_decoder}
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
                  checked={formData.enable_display}
                  onCheckedChange={(checked) => setFormData({...formData, enable_display: checked})}
                />
              </div>
            </CardContent>
          </Card>

          {/* Motion Detection */}
          <Card>
            <CardHeader>
              <CardTitle>Motion Detection</CardTitle>
              <CardDescription>Configure motion detection settings</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between pb-4 border-b">
                <div>
                  <Label htmlFor="motion_enabled">Enable Motion Detection</Label>
                  <p className="text-sm text-muted-foreground">Detect motion in camera feed</p>
                </div>
                <Switch
                  id="motion_enabled"
                  checked={formData.motion_detection?.enabled}
                  onCheckedChange={(checked) =>
                    setFormData({
                      ...formData,
                      motion_detection: {...formData.motion_detection!, enabled: checked}
                    })
                  }
                />
              </div>

              {formData.motion_detection?.enabled && (
                <div className="space-y-4 pt-4">
                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="algorithm">Algorithm</Label>
                      <Select
                        value={formData.motion_detection.algorithm}
                        onValueChange={(value) =>
                          setFormData({
                            ...formData,
                            motion_detection: {...formData.motion_detection!, algorithm: value}
                          })
                        }
                      >
                        <SelectTrigger id="algorithm">
                          <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                          <SelectItem value="MOG2_CUDA">MOG2 CUDA (GPU)</SelectItem>
                          <SelectItem value="MOG2">MOG2 (CPU)</SelectItem>
                          <SelectItem value="KNN">KNN</SelectItem>
                          <SelectItem value="FRAME_DIFF">Frame Difference</SelectItem>
                        </SelectContent>
                      </Select>
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="sensitivity">Sensitivity</Label>
                      <Input
                        id="sensitivity"
                        type="number"
                        step="0.1"
                        min="0"
                        max="1"
                        value={formData.motion_detection.sensitivity}
                        onChange={(e) =>
                          setFormData({
                            ...formData,
                            motion_detection: {...formData.motion_detection!, sensitivity: parseFloat(e.target.value)}
                          })
                        }
                      />
                    </div>
                  </div>

                  <div className="grid grid-cols-2 gap-4">
                    <div className="space-y-2">
                      <Label htmlFor="min_contour_area">Min Contour Area</Label>
                      <Input
                        id="min_contour_area"
                        type="number"
                        value={formData.motion_detection.min_contour_area}
                        onChange={(e) =>
                          setFormData({
                            ...formData,
                            motion_detection: {...formData.motion_detection!, min_contour_area: parseInt(e.target.value)}
                          })
                        }
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="cooldown_seconds">Cooldown (seconds)</Label>
                      <Input
                        id="cooldown_seconds"
                        type="number"
                        step="0.1"
                        value={formData.motion_detection.cooldown_seconds}
                        onChange={(e) =>
                          setFormData({
                            ...formData,
                            motion_detection: {...formData.motion_detection!, cooldown_seconds: parseFloat(e.target.value)}
                          })
                        }
                      />
                    </div>
                  </div>
                </div>
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
            <Button type="submit" disabled={loading}>
              {loading ? (
                <><Loader2 className="h-4 w-4 mr-2 animate-spin" />Creating...</>
              ) : (
                <><Save className="h-4 w-4 mr-2" />Create Camera</>
              )}
            </Button>
          </div>
        </form>
      </main>
    </div>
  );
}
