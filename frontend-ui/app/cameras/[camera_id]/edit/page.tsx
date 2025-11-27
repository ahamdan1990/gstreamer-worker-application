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

      // Fetch face detection configuration separately
      let faceDetectionConfig = null;
      try {
        const fdResponse = await fetch(`http://localhost:8002/api/v1/cameras/${cameraId}/face-detection`);
        if (fdResponse.ok) {
          const fdData = await fdResponse.json();
          faceDetectionConfig = fdData.config || null;
        }
      } catch (error) {
        console.warn('Failed to load face detection config:', error);
      }

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
        },
        face_detection: faceDetectionConfig ? {
          ...faceDetectionConfig
        } : {
          enabled: false,
          model_path: "models/scrfd/scrfd_10g_bnkps.onnx",
          input_size: 640,
          confidence_threshold: 0.3,
          nms_threshold: 0.5,
          frame_skip: 2,
          max_frame_width: 1920,
          max_frame_height: 1080,
          min_face_size: 0.0003,
          max_faces: 30,
          required_frames: 1,
          cooldown_seconds: 0.3,
          use_tensorrt: true,
          use_cuda: true,
          max_batch_size: 4,
          save_faces: true,
          save_path: "./face_crops",
          save_margin: 0.3,
          min_save_confidence: 0.2,
          max_saves_per_event: 5,
          enable_blur_detection: true,
          min_laplacian_variance: 250.0,
          blur_kernel_size: 3,
          motion_triggered_detection: true,
          motion_detection_cooldown: 1.5,
          enable_compreface: true,
          compreface_url: "http://localhost:8000",
          compreface_api_key: "318c40d0-2142-40b9-b8ec-59d12acc157d",
          compreface_subject: "unknown",
          compreface_timeout_ms: 8000,
          compreface_max_queue_size: 200,
          enable_visualization: true,
          draw_landmarks: true,
          draw_confidence: true,
          box_thickness: 2,
          font_scale: 0.5
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

      // Save main camera configuration
      await apiClient.updateCamera(cameraId, formData);

      // Save face detection configuration separately if present
      if (formData.face_detection) {
        const faceDetectionFormData = new FormData();
        faceDetectionFormData.append('enabled', String(formData.face_detection.enabled ?? false));
        faceDetectionFormData.append('config', JSON.stringify(formData.face_detection));

        await fetch(`http://localhost:8002/api/v1/cameras/${cameraId}/face-detection`, {
          method: 'PUT',
          body: faceDetectionFormData,
        });
      }

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

          {/* Face Detection Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Face Detection Configuration</CardTitle>
              <CardDescription>Configure face detection with SCRFD model and CompreFace integration</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="face_enabled">Enable Face Detection</Label>
                  <p className="text-sm text-muted-foreground">Detect faces using SCRFD model</p>
                </div>
                <Switch
                  id="face_enabled"
                  checked={formData.face_detection?.enabled ?? false}
                  onCheckedChange={(checked) => setFormData({
                    ...formData,
                    face_detection: {
                      ...(formData.face_detection || {}),
                      enabled: checked
                    }
                  })}
                />
              </div>

              {formData.face_detection?.enabled && (
                <>
                  {/* Model Configuration */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Model Configuration</h4>
                    <div className="space-y-4">
                      <div className="grid grid-cols-2 gap-4">
                        <div className="space-y-2">
                          <Label htmlFor="fd_model_path">Model Path</Label>
                          <Input
                            id="fd_model_path"
                            value={formData.face_detection?.model_path ?? ''}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                model_path: e.target.value
                              }
                            })}
                          />
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_input_size">Input Size (px)</Label>
                          <Input
                            id="fd_input_size"
                            type="number"
                            value={formData.face_detection?.input_size ?? 640}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                input_size: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>

                      <div className="grid grid-cols-2 gap-4">
                        <div className="space-y-2">
                          <Label htmlFor="fd_confidence">Confidence Threshold (0-1)</Label>
                          <Input
                            id="fd_confidence"
                            type="number"
                            step="0.01"
                            min="0"
                            max="1"
                            value={formData.face_detection?.confidence_threshold ?? 0.3}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                confidence_threshold: parseFloat(e.target.value)
                              }
                            })}
                          />
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_nms">NMS Threshold (0-1)</Label>
                          <Input
                            id="fd_nms"
                            type="number"
                            step="0.01"
                            min="0"
                            max="1"
                            value={formData.face_detection?.nms_threshold ?? 0.5}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                nms_threshold: parseFloat(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Frame Processing */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Frame Processing</h4>
                    <div className="space-y-4">
                      <div className="grid grid-cols-3 gap-4">
                        <div className="space-y-2">
                          <Label htmlFor="fd_frame_skip">Frame Skip</Label>
                          <Input
                            id="fd_frame_skip"
                            type="number"
                            value={formData.face_detection?.frame_skip ?? 2}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                frame_skip: parseInt(e.target.value)
                              }
                            })}
                          />
                          <p className="text-xs text-muted-foreground">Process 1 in N frames</p>
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_max_width">Max Frame Width</Label>
                          <Input
                            id="fd_max_width"
                            type="number"
                            value={formData.face_detection?.max_frame_width ?? 1920}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                max_frame_width: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_max_height">Max Frame Height</Label>
                          <Input
                            id="fd_max_height"
                            type="number"
                            value={formData.face_detection?.max_frame_height ?? 1080}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                max_frame_height: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Face Detection Parameters */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Face Detection Parameters</h4>
                    <div className="space-y-4">
                      <div className="grid grid-cols-2 gap-4">
                        <div className="space-y-2">
                          <Label htmlFor="fd_min_face_size">Min Face Size (ratio)</Label>
                          <Input
                            id="fd_min_face_size"
                            type="number"
                            step="0.0001"
                            value={formData.face_detection?.min_face_size ?? 0.0003}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                min_face_size: parseFloat(e.target.value)
                              }
                            })}
                          />
                          <p className="text-xs text-muted-foreground">Relative to frame area</p>
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_max_faces">Max Faces Per Frame</Label>
                          <Input
                            id="fd_max_faces"
                            type="number"
                            value={formData.face_detection?.max_faces ?? 30}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                max_faces: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>

                      <div className="grid grid-cols-2 gap-4">
                        <div className="space-y-2">
                          <Label htmlFor="fd_required_frames">Required Frames</Label>
                          <Input
                            id="fd_required_frames"
                            type="number"
                            value={formData.face_detection?.required_frames ?? 1}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                required_frames: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_cooldown">Cooldown (seconds)</Label>
                          <Input
                            id="fd_cooldown"
                            type="number"
                            step="0.1"
                            value={formData.face_detection?.cooldown_seconds ?? 0.3}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                cooldown_seconds: parseFloat(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* TensorRT/CUDA Settings */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">TensorRT/CUDA Settings</h4>
                    <div className="space-y-4">
                      <div className="grid grid-cols-3 gap-4">
                        <div className="flex items-center justify-between">
                          <Label htmlFor="fd_tensorrt">Use TensorRT</Label>
                          <Switch
                            id="fd_tensorrt"
                            checked={formData.face_detection?.use_tensorrt ?? true}
                            onCheckedChange={(checked) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                use_tensorrt: checked
                              }
                            })}
                          />
                        </div>

                        <div className="flex items-center justify-between">
                          <Label htmlFor="fd_cuda">Use CUDA</Label>
                          <Switch
                            id="fd_cuda"
                            checked={formData.face_detection?.use_cuda ?? true}
                            onCheckedChange={(checked) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                use_cuda: checked
                              }
                            })}
                          />
                        </div>

                        <div className="space-y-2">
                          <Label htmlFor="fd_batch_size">Max Batch Size</Label>
                          <Input
                            id="fd_batch_size"
                            type="number"
                            value={formData.face_detection?.max_batch_size ?? 4}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                max_batch_size: parseInt(e.target.value)
                              }
                            })}
                          />
                        </div>
                      </div>
                    </div>
                  </div>

                  {/* Face Saving Configuration */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Face Saving Configuration</h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <Label htmlFor="fd_save_faces">Save Face Crops</Label>
                          <p className="text-sm text-muted-foreground">Save detected faces to disk</p>
                        </div>
                        <Switch
                          id="fd_save_faces"
                          checked={formData.face_detection?.save_faces ?? true}
                          onCheckedChange={(checked) => setFormData({
                            ...formData,
                            face_detection: {
                              ...(formData.face_detection || {}),
                              save_faces: checked
                            }
                          })}
                        />
                      </div>

                      {formData.face_detection?.save_faces && (
                        <>
                          <div className="space-y-2">
                            <Label htmlFor="fd_save_path">Save Path</Label>
                            <Input
                              id="fd_save_path"
                              value={formData.face_detection?.save_path ?? './face_crops'}
                              onChange={(e) => setFormData({
                                ...formData,
                                face_detection: {
                                  ...(formData.face_detection || {}),
                                  save_path: e.target.value
                                }
                              })}
                            />
                          </div>

                          <div className="grid grid-cols-3 gap-4">
                            <div className="space-y-2">
                              <Label htmlFor="fd_save_margin">Save Margin</Label>
                              <Input
                                id="fd_save_margin"
                                type="number"
                                step="0.1"
                                value={formData.face_detection?.save_margin ?? 0.3}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    save_margin: parseFloat(e.target.value)
                                  }
                                })}
                              />
                            </div>

                            <div className="space-y-2">
                              <Label htmlFor="fd_min_save_conf">Min Save Confidence</Label>
                              <Input
                                id="fd_min_save_conf"
                                type="number"
                                step="0.01"
                                min="0"
                                max="1"
                                value={formData.face_detection?.min_save_confidence ?? 0.2}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    min_save_confidence: parseFloat(e.target.value)
                                  }
                                })}
                              />
                            </div>

                            <div className="space-y-2">
                              <Label htmlFor="fd_max_saves">Max Saves Per Event</Label>
                              <Input
                                id="fd_max_saves"
                                type="number"
                                value={formData.face_detection?.max_saves_per_event ?? 5}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    max_saves_per_event: parseInt(e.target.value)
                                  }
                                })}
                              />
                            </div>
                          </div>
                        </>
                      )}
                    </div>
                  </div>

                  {/* Blur Detection */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Blur Detection</h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <Label htmlFor="fd_blur">Enable Blur Detection</Label>
                          <p className="text-sm text-muted-foreground">Filter out blurry face images</p>
                        </div>
                        <Switch
                          id="fd_blur"
                          checked={formData.face_detection?.enable_blur_detection ?? true}
                          onCheckedChange={(checked) => setFormData({
                            ...formData,
                            face_detection: {
                              ...(formData.face_detection || {}),
                              enable_blur_detection: checked
                            }
                          })}
                        />
                      </div>

                      {formData.face_detection?.enable_blur_detection && (
                        <div className="grid grid-cols-2 gap-4">
                          <div className="space-y-2">
                            <Label htmlFor="fd_laplacian">Min Laplacian Variance</Label>
                            <Input
                              id="fd_laplacian"
                              type="number"
                              step="10"
                              value={formData.face_detection?.min_laplacian_variance ?? 250}
                              onChange={(e) => setFormData({
                                ...formData,
                                face_detection: {
                                  ...(formData.face_detection || {}),
                                  min_laplacian_variance: parseFloat(e.target.value)
                                }
                              })}
                            />
                            <p className="text-xs text-muted-foreground">Higher = more strict</p>
                          </div>

                          <div className="space-y-2">
                            <Label htmlFor="fd_blur_kernel">Blur Kernel Size</Label>
                            <Input
                              id="fd_blur_kernel"
                              type="number"
                              value={formData.face_detection?.blur_kernel_size ?? 3}
                              onChange={(e) => setFormData({
                                ...formData,
                                face_detection: {
                                  ...(formData.face_detection || {}),
                                  blur_kernel_size: parseInt(e.target.value)
                                }
                              })}
                            />
                          </div>
                        </div>
                      )}
                    </div>
                  </div>

                  {/* Motion Integration */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Motion Integration</h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <Label htmlFor="fd_motion_triggered">Motion-Triggered Detection</Label>
                          <p className="text-sm text-muted-foreground">Only detect faces when motion is detected</p>
                        </div>
                        <Switch
                          id="fd_motion_triggered"
                          checked={formData.face_detection?.motion_triggered_detection ?? true}
                          onCheckedChange={(checked) => setFormData({
                            ...formData,
                            face_detection: {
                              ...(formData.face_detection || {}),
                              motion_triggered_detection: checked
                            }
                          })}
                        />
                      </div>

                      {formData.face_detection?.motion_triggered_detection && (
                        <div className="space-y-2">
                          <Label htmlFor="fd_motion_cooldown">Motion Detection Cooldown (seconds)</Label>
                          <Input
                            id="fd_motion_cooldown"
                            type="number"
                            step="0.1"
                            value={formData.face_detection?.motion_detection_cooldown ?? 1.5}
                            onChange={(e) => setFormData({
                              ...formData,
                              face_detection: {
                                ...(formData.face_detection || {}),
                                motion_detection_cooldown: parseFloat(e.target.value)
                              }
                            })}
                          />
                        </div>
                      )}
                    </div>
                  </div>

                  {/* CompreFace Integration */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">CompreFace Integration</h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <Label htmlFor="fd_compreface">Enable CompreFace</Label>
                          <p className="text-sm text-muted-foreground">Send faces to CompreFace for recognition</p>
                        </div>
                        <Switch
                          id="fd_compreface"
                          checked={formData.face_detection?.enable_compreface ?? true}
                          onCheckedChange={(checked) => setFormData({
                            ...formData,
                            face_detection: {
                              ...(formData.face_detection || {}),
                              enable_compreface: checked
                            }
                          })}
                        />
                      </div>

                      {formData.face_detection?.enable_compreface && (
                        <>
                          <div className="grid grid-cols-2 gap-4">
                            <div className="space-y-2">
                              <Label htmlFor="fd_cf_url">CompreFace URL</Label>
                              <Input
                                id="fd_cf_url"
                                value={formData.face_detection?.compreface_url ?? 'http://localhost:8000'}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    compreface_url: e.target.value
                                  }
                                })}
                              />
                            </div>

                            <div className="space-y-2">
                              <Label htmlFor="fd_cf_subject">Default Subject</Label>
                              <Input
                                id="fd_cf_subject"
                                value={formData.face_detection?.compreface_subject ?? 'unknown'}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    compreface_subject: e.target.value
                                  }
                                })}
                              />
                            </div>
                          </div>

                          <div className="space-y-2">
                            <Label htmlFor="fd_cf_key">API Key</Label>
                            <Input
                              id="fd_cf_key"
                              type="password"
                              value={formData.face_detection?.compreface_api_key ?? ''}
                              onChange={(e) => setFormData({
                                ...formData,
                                face_detection: {
                                  ...(formData.face_detection || {}),
                                  compreface_api_key: e.target.value
                                }
                              })}
                            />
                          </div>

                          <div className="grid grid-cols-2 gap-4">
                            <div className="space-y-2">
                              <Label htmlFor="fd_cf_timeout">Timeout (ms)</Label>
                              <Input
                                id="fd_cf_timeout"
                                type="number"
                                value={formData.face_detection?.compreface_timeout_ms ?? 8000}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    compreface_timeout_ms: parseInt(e.target.value)
                                  }
                                })}
                              />
                            </div>

                            <div className="space-y-2">
                              <Label htmlFor="fd_cf_queue">Max Queue Size</Label>
                              <Input
                                id="fd_cf_queue"
                                type="number"
                                value={formData.face_detection?.compreface_max_queue_size ?? 200}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    compreface_max_queue_size: parseInt(e.target.value)
                                  }
                                })}
                              />
                            </div>
                          </div>
                        </>
                      )}
                    </div>
                  </div>

                  {/* Visualization Settings */}
                  <div className="border-t pt-4 mt-4">
                    <h4 className="font-semibold mb-3">Visualization Settings</h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <Label htmlFor="fd_viz">Enable Visualization</Label>
                          <p className="text-sm text-muted-foreground">Draw bounding boxes and landmarks on display</p>
                        </div>
                        <Switch
                          id="fd_viz"
                          checked={formData.face_detection?.enable_visualization ?? true}
                          onCheckedChange={(checked) => setFormData({
                            ...formData,
                            face_detection: {
                              ...(formData.face_detection || {}),
                              enable_visualization: checked
                            }
                          })}
                        />
                      </div>

                      {formData.face_detection?.enable_visualization && (
                        <>
                          <div className="grid grid-cols-2 gap-4">
                            <div className="flex items-center justify-between">
                              <Label htmlFor="fd_landmarks">Draw Landmarks</Label>
                              <Switch
                                id="fd_landmarks"
                                checked={formData.face_detection?.draw_landmarks ?? true}
                                onCheckedChange={(checked) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    draw_landmarks: checked
                                  }
                                })}
                              />
                            </div>

                            <div className="flex items-center justify-between">
                              <Label htmlFor="fd_conf">Draw Confidence</Label>
                              <Switch
                                id="fd_conf"
                                checked={formData.face_detection?.draw_confidence ?? true}
                                onCheckedChange={(checked) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    draw_confidence: checked
                                  }
                                })}
                              />
                            </div>
                          </div>

                          <div className="grid grid-cols-2 gap-4">
                            <div className="space-y-2">
                              <Label htmlFor="fd_box_thickness">Box Thickness</Label>
                              <Input
                                id="fd_box_thickness"
                                type="number"
                                value={formData.face_detection?.box_thickness ?? 2}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    box_thickness: parseInt(e.target.value)
                                  }
                                })}
                              />
                            </div>

                            <div className="space-y-2">
                              <Label htmlFor="fd_font_scale">Font Scale</Label>
                              <Input
                                id="fd_font_scale"
                                type="number"
                                step="0.1"
                                value={formData.face_detection?.font_scale ?? 0.5}
                                onChange={(e) => setFormData({
                                  ...formData,
                                  face_detection: {
                                    ...(formData.face_detection || {}),
                                    font_scale: parseFloat(e.target.value)
                                  }
                                })}
                              />
                            </div>
                          </div>
                        </>
                      )}
                    </div>
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
