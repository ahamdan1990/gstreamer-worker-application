'use client';

import { useEffect, useState, useRef } from 'react';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Switch } from '@/components/ui/switch';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import {
  Settings as SettingsIcon,
  Save,
  RotateCcw,
  Users,
  Activity,
  Clock,
  Database,
  TrendingUp,
  AlertCircle,
  FileText,
  ScanFace,
} from 'lucide-react';
import { Layout } from '@/components/layout';
import { Alert, AlertDescription } from '@/components/ui/alert';

// Event categories for log filtering
const EVENT_CATEGORIES = {
  motion: ['motion_detected'],
  face: ['face_detected', 'person_recognized'],
  camera: ['camera_started', 'camera_stopped', 'camera_error'],
  system: ['worker_started', 'worker_stopped', 'pipeline_crashed', 'fps_drop'],
  logs: ['log', 'INFO', 'DEBUG', 'WARNING', 'ERROR']
};

// Get all possible event types
const ALL_EVENT_TYPES = Object.values(EVENT_CATEGORIES).flat();

export default function SettingsPage() {
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [successMessage, setSuccessMessage] = useState('');
  const [errorMessage, setErrorMessage] = useState('');

  // Logs Settings
  const [excludedTypes, setExcludedTypes] = useState<Set<string>>(new Set());
  const isInitialMount = useRef(true);

  // Person Tracking Settings
  const [personTracking, setPersonTracking] = useState({
    enabled: true,
    presence_timeout_seconds: 300,
    same_camera_cooldown_seconds: 10,
    max_detections_per_person: 100,
    enable_cross_camera_tracking: true,
    enable_persistence: true,
    persistence_path: './person_tracking.json',
    enable_daily_reset: true,
    daily_reset_hour: 0,
    archive_path: './tracking_archives',
  });

  // Manager Settings
  const [manager, setManager] = useState({
    log_level: 'INFO',
    enable_metrics: true,
    metrics_interval: 30,
  });

  // CompreFace Recognition Settings
  const [comprefaceRecognition, setComprefaceRecognition] = useState({
    similarity_threshold: 0.88,
    description: '',
  });

  useEffect(() => {
    loadSettings();
  }, []);

  // Load excluded log types from localStorage
  useEffect(() => {
    const saved = localStorage.getItem('excludedLogTypes');
    if (saved) {
      setExcludedTypes(new Set(JSON.parse(saved)));
    }
  }, []);

  // Save excluded types to localStorage when changed (skip on initial mount)
  useEffect(() => {
    if (isInitialMount.current) {
      isInitialMount.current = false; // Mark as no longer initial mount
      return; // Don't save on initial mount
    }
    localStorage.setItem('excludedLogTypes', JSON.stringify([...excludedTypes]));
  }, [excludedTypes]);

  const loadSettings = async () => {
    try {
      setLoading(true);
      setErrorMessage('');

      // Load person tracking settings
      const ptResponse = await fetch('http://192.168.0.24:8002/api/v1/settings/person-tracking/config');
      if (ptResponse.ok) {
        const ptData = await ptResponse.json();
        setPersonTracking(ptData);
      }

      // Load manager settings
      const mgResponse = await fetch('http://192.168.0.24:8002/api/v1/settings/manager/config');
      if (mgResponse.ok) {
        const mgData = await mgResponse.json();
        setManager(mgData);
      }

      // Load CompreFace recognition settings
      const cfResponse = await fetch('http://192.168.0.24:8002/api/v1/settings/compreface-recognition/config');
      if (cfResponse.ok) {
        const cfData = await cfResponse.json();
        setComprefaceRecognition(cfData);
      }
    } catch (error) {
      console.error('Failed to load settings:', error);
      setErrorMessage('Failed to load settings. Make sure the backend server is running.');
    } finally {
      setLoading(false);
    }
  };

  const savePersonTrackingSettings = async () => {
    try {
      setSaving(true);
      setErrorMessage('');
      setSuccessMessage('');

      const formData = new FormData();
      formData.append('enabled', personTracking.enabled.toString());
      formData.append('presence_timeout_seconds', personTracking.presence_timeout_seconds.toString());
      formData.append('same_camera_cooldown_seconds', personTracking.same_camera_cooldown_seconds.toString());
      formData.append('max_detections_per_person', personTracking.max_detections_per_person.toString());
      formData.append('enable_cross_camera_tracking', personTracking.enable_cross_camera_tracking.toString());
      formData.append('enable_persistence', personTracking.enable_persistence.toString());
      formData.append('persistence_path', personTracking.persistence_path);
      formData.append('enable_daily_reset', personTracking.enable_daily_reset.toString());
      formData.append('daily_reset_hour', personTracking.daily_reset_hour.toString());
      formData.append('archive_path', personTracking.archive_path);

      const response = await fetch('http://192.168.0.24:8002/api/v1/settings/person-tracking/config', {
        method: 'PUT',
        body: formData,
      });

      if (response.ok) {
        setSuccessMessage('Person tracking settings saved successfully!');
        setTimeout(() => setSuccessMessage(''), 3000);
      } else {
        throw new Error('Failed to save settings');
      }
    } catch (error) {
      console.error('Failed to save person tracking settings:', error);
      setErrorMessage('Failed to save person tracking settings');
    } finally {
      setSaving(false);
    }
  };

  const saveManagerSettings = async () => {
    try {
      setSaving(true);
      setErrorMessage('');
      setSuccessMessage('');

      const formData = new FormData();
      formData.append('log_level', manager.log_level);
      formData.append('enable_metrics', manager.enable_metrics.toString());
      formData.append('metrics_interval', manager.metrics_interval.toString());

      const response = await fetch('http://192.168.0.24:8002/api/v1/settings/manager/config', {
        method: 'PUT',
        body: formData,
      });

      if (response.ok) {
        setSuccessMessage('Manager settings saved successfully!');
        setTimeout(() => setSuccessMessage(''), 3000);
      } else {
        throw new Error('Failed to save settings');
      }
    } catch (error) {
      console.error('Failed to save manager settings:', error);
      setErrorMessage('Failed to save manager settings');
    } finally {
      setSaving(false);
    }
  };

  const saveComprefaceRecognitionSettings = async () => {
    try {
      setSaving(true);
      setErrorMessage('');
      setSuccessMessage('');

      const formData = new FormData();
      formData.append('similarity_threshold', comprefaceRecognition.similarity_threshold.toString());

      const response = await fetch('http://192.168.0.24:8002/api/v1/settings/compreface-recognition/config', {
        method: 'PUT',
        body: formData,
      });

      if (response.ok) {
        setSuccessMessage('CompreFace recognition settings saved successfully! New cameras will use this threshold.');
        setTimeout(() => setSuccessMessage(''), 3000);
      } else {
        throw new Error('Failed to save settings');
      }
    } catch (error) {
      console.error('Failed to save CompreFace settings:', error);
      setErrorMessage('Failed to save CompreFace recognition settings');
    } finally {
      setSaving(false);
    }
  };

  const resetPersonTracking = async () => {
    if (!confirm('Reset person tracking settings to defaults?')) return;

    try {
      setSaving(true);
      const response = await fetch('http://192.168.0.24:8002/api/v1/settings/person_tracking/reset', {
        method: 'POST',
      });

      if (response.ok) {
        await loadSettings();
        setSuccessMessage('Person tracking settings reset to defaults!');
        setTimeout(() => setSuccessMessage(''), 3000);
      }
    } catch (error) {
      console.error('Failed to reset settings:', error);
      setErrorMessage('Failed to reset settings');
    } finally {
      setSaving(false);
    }
  };

  const resetManager = async () => {
    if (!confirm('Reset manager settings to defaults?')) return;

    try {
      setSaving(true);
      const response = await fetch('http://192.168.0.24:8002/api/v1/settings/manager/reset', {
        method: 'POST',
      });

      if (response.ok) {
        await loadSettings();
        setSuccessMessage('Manager settings reset to defaults!');
        setTimeout(() => setSuccessMessage(''), 3000);
      }
    } catch (error) {
      console.error('Failed to reset settings:', error);
      setErrorMessage('Failed to reset settings');
    } finally {
      setSaving(false);
    }
  };

  const toggleExcludedType = (type: string) => {
    setExcludedTypes((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(type)) {
        newSet.delete(type);
      } else {
        newSet.add(type);
      }
      return newSet;
    });
  };

  if (loading) {
    return (
      <Layout>
        <div className="flex items-center justify-center min-h-screen">
          <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
        </div>
      </Layout>
    );
  }

  return (
    <Layout>
      <div className="container mx-auto py-8 px-4 max-w-7xl">
        {/* Page Header */}
        <div className="mb-6">
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-2">
            <SettingsIcon className="h-8 w-8 text-blue-600" />
            System Settings
          </h1>
          <p className="text-gray-500 mt-1">
            Configure global system settings for person tracking and manager
          </p>
        </div>

        {/* Success/Error Messages */}
        {successMessage && (
          <Alert className="mb-6 bg-green-50 border-green-200">
            <AlertCircle className="h-4 w-4 text-green-600" />
            <AlertDescription className="text-green-800">{successMessage}</AlertDescription>
          </Alert>
        )}

        {errorMessage && (
          <Alert className="mb-6 bg-red-50 border-red-200">
            <AlertCircle className="h-4 w-4 text-red-600" />
            <AlertDescription className="text-red-800">{errorMessage}</AlertDescription>
          </Alert>
        )}

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Person Tracking Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Users className="h-5 w-5 text-blue-600" />
                Person Tracking
              </CardTitle>
              <CardDescription>
                Configure cross-camera person tracking and persistence
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-6">
              {/* Enable/Disable */}
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="pt-enabled" className="font-semibold">
                    Enable Person Tracking
                  </Label>
                  <p className="text-sm text-gray-500">Track persons across cameras</p>
                </div>
                <Switch
                  id="pt-enabled"
                  checked={personTracking.enabled}
                  onCheckedChange={(checked) =>
                    setPersonTracking({ ...personTracking, enabled: checked })
                  }
                />
              </div>

              {/* Presence Timeout */}
              <div className="space-y-2">
                <Label htmlFor="pt-presence-timeout" className="flex items-center gap-2">
                  <Clock className="h-4 w-4" />
                  Presence Timeout (seconds)
                </Label>
                <Input
                  id="pt-presence-timeout"
                  type="number"
                  value={personTracking.presence_timeout_seconds}
                  onChange={(e) =>
                    setPersonTracking({
                      ...personTracking,
                      presence_timeout_seconds: parseFloat(e.target.value),
                    })
                  }
                />
                <p className="text-xs text-gray-500">
                  Time before person is marked as left (default: 300)
                </p>
              </div>

              {/* Camera Cooldown */}
              <div className="space-y-2">
                <Label htmlFor="pt-cooldown">Same Camera Cooldown (seconds)</Label>
                <Input
                  id="pt-cooldown"
                  type="number"
                  value={personTracking.same_camera_cooldown_seconds}
                  onChange={(e) =>
                    setPersonTracking({
                      ...personTracking,
                      same_camera_cooldown_seconds: parseFloat(e.target.value),
                    })
                  }
                />
                <p className="text-xs text-gray-500">
                  Cooldown between recognitions on same camera (default: 10)
                </p>
              </div>

              {/* Max Detections */}
              <div className="space-y-2">
                <Label htmlFor="pt-max-detections">Max Detections Per Person</Label>
                <Input
                  id="pt-max-detections"
                  type="number"
                  value={personTracking.max_detections_per_person}
                  onChange={(e) =>
                    setPersonTracking({
                      ...personTracking,
                      max_detections_per_person: parseInt(e.target.value),
                    })
                  }
                />
                <p className="text-xs text-gray-500">
                  Limit to prevent memory issues (default: 100)
                </p>
              </div>

              {/* Cross-Camera Tracking */}
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="pt-cross-camera" className="font-semibold">
                    Cross-Camera Tracking
                  </Label>
                  <p className="text-sm text-gray-500">Track persons across multiple cameras</p>
                </div>
                <Switch
                  id="pt-cross-camera"
                  checked={personTracking.enable_cross_camera_tracking}
                  onCheckedChange={(checked) =>
                    setPersonTracking({ ...personTracking, enable_cross_camera_tracking: checked })
                  }
                />
              </div>

              {/* Persistence */}
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="pt-persistence" className="font-semibold flex items-center gap-2">
                    <Database className="h-4 w-4" />
                    Enable Persistence
                  </Label>
                  <p className="text-sm text-gray-500">Save tracking state to disk</p>
                </div>
                <Switch
                  id="pt-persistence"
                  checked={personTracking.enable_persistence}
                  onCheckedChange={(checked) =>
                    setPersonTracking({ ...personTracking, enable_persistence: checked })
                  }
                />
              </div>

              {/* Persistence Path */}
              {personTracking.enable_persistence && (
                <div className="space-y-2">
                  <Label htmlFor="pt-persistence-path">Persistence Path</Label>
                  <Input
                    id="pt-persistence-path"
                    value={personTracking.persistence_path}
                    onChange={(e) =>
                      setPersonTracking({ ...personTracking, persistence_path: e.target.value })
                    }
                  />
                </div>
              )}

              {/* Daily Reset */}
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="pt-daily-reset" className="font-semibold">
                    Daily Reset
                  </Label>
                  <p className="text-sm text-gray-500">Reset tracking at midnight</p>
                </div>
                <Switch
                  id="pt-daily-reset"
                  checked={personTracking.enable_daily_reset}
                  onCheckedChange={(checked) =>
                    setPersonTracking({ ...personTracking, enable_daily_reset: checked })
                  }
                />
              </div>

              {/* Reset Hour */}
              {personTracking.enable_daily_reset && (
                <div className="space-y-2">
                  <Label htmlFor="pt-reset-hour">Reset Hour (0-23)</Label>
                  <Input
                    id="pt-reset-hour"
                    type="number"
                    min="0"
                    max="23"
                    value={personTracking.daily_reset_hour}
                    onChange={(e) =>
                      setPersonTracking({
                        ...personTracking,
                        daily_reset_hour: parseInt(e.target.value),
                      })
                    }
                  />
                </div>
              )}

              {/* Archive Path */}
              <div className="space-y-2">
                <Label htmlFor="pt-archive-path">Archive Path</Label>
                <Input
                  id="pt-archive-path"
                  value={personTracking.archive_path}
                  onChange={(e) =>
                    setPersonTracking({ ...personTracking, archive_path: e.target.value })
                  }
                />
                <p className="text-xs text-gray-500">Where to archive old tracking data</p>
              </div>

              {/* Action Buttons */}
              <div className="flex gap-2 pt-4">
                <Button
                  onClick={savePersonTrackingSettings}
                  disabled={saving}
                  className="flex-1"
                >
                  <Save className="h-4 w-4 mr-2" />
                  Save Person Tracking
                </Button>
                <Button onClick={resetPersonTracking} variant="outline" disabled={saving}>
                  <RotateCcw className="h-4 w-4 mr-2" />
                  Reset
                </Button>
              </div>
            </CardContent>
          </Card>

          {/* Manager Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Activity className="h-5 w-5 text-blue-600" />
                Manager Configuration
              </CardTitle>
              <CardDescription>
                System manager logging and metrics settings
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-6">
              {/* Log Level */}
              <div className="space-y-2">
                <Label htmlFor="mg-log-level">Log Level</Label>
                <Select
                  value={manager.log_level}
                  onValueChange={(value) => setManager({ ...manager, log_level: value })}
                >
                  <SelectTrigger id="mg-log-level">
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="DEBUG">DEBUG</SelectItem>
                    <SelectItem value="INFO">INFO</SelectItem>
                    <SelectItem value="WARNING">WARNING</SelectItem>
                    <SelectItem value="ERROR">ERROR</SelectItem>
                    <SelectItem value="CRITICAL">CRITICAL</SelectItem>
                  </SelectContent>
                </Select>
                <p className="text-xs text-gray-500">
                  Verbosity level for system logs (default: INFO)
                </p>
              </div>

              {/* Enable Metrics */}
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="mg-metrics" className="font-semibold flex items-center gap-2">
                    <TrendingUp className="h-4 w-4" />
                    Enable Metrics
                  </Label>
                  <p className="text-sm text-gray-500">Collect system performance metrics</p>
                </div>
                <Switch
                  id="mg-metrics"
                  checked={manager.enable_metrics}
                  onCheckedChange={(checked) =>
                    setManager({ ...manager, enable_metrics: checked })
                  }
                />
              </div>

              {/* Metrics Interval */}
              {manager.enable_metrics && (
                <div className="space-y-2">
                  <Label htmlFor="mg-interval">Metrics Collection Interval (seconds)</Label>
                  <Input
                    id="mg-interval"
                    type="number"
                    value={manager.metrics_interval}
                    onChange={(e) =>
                      setManager({ ...manager, metrics_interval: parseFloat(e.target.value) })
                    }
                  />
                  <p className="text-xs text-gray-500">
                    How often to collect metrics (default: 30)
                  </p>
                </div>
              )}

              {/* Info Box */}
              <Alert>
                <AlertCircle className="h-4 w-4" />
                <AlertDescription>
                  <strong>Note:</strong> Changing manager settings may require restarting the
                  worker process to take effect.
                </AlertDescription>
              </Alert>

              {/* Action Buttons */}
              <div className="flex gap-2 pt-4">
                <Button onClick={saveManagerSettings} disabled={saving} className="flex-1">
                  <Save className="h-4 w-4 mr-2" />
                  Save Manager Settings
                </Button>
                <Button onClick={resetManager} variant="outline" disabled={saving}>
                  <RotateCcw className="h-4 w-4 mr-2" />
                  Reset
                </Button>
              </div>
            </CardContent>
          </Card>

          {/* CompreFace Recognition Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <ScanFace className="h-5 w-5 text-blue-600" />
                Face Recognition
              </CardTitle>
              <CardDescription>
                Configure CompreFace face recognition matching threshold
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-6">
              {/* Similarity Threshold */}
              <div className="space-y-4">
                <div className="flex items-center justify-between">
                  <Label htmlFor="cf-similarity" className="font-semibold">
                    Similarity Threshold
                  </Label>
                  <span className="text-2xl font-bold text-blue-600">
                    {Math.round(comprefaceRecognition.similarity_threshold * 100)}%
                  </span>
                </div>

                <input
                  id="cf-similarity"
                  type="range"
                  min="0"
                  max="100"
                  step="1"
                  value={comprefaceRecognition.similarity_threshold * 100}
                  onChange={(e) =>
                    setComprefaceRecognition({
                      ...comprefaceRecognition,
                      similarity_threshold: parseFloat(e.target.value) / 100,
                    })
                  }
                  className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer accent-blue-600"
                />

                <div className="flex justify-between text-xs text-gray-500">
                  <span>0% (Match All)</span>
                  <span>50% (Moderate)</span>
                  <span>100% (Exact Match)</span>
                </div>

                <Alert>
                  <AlertCircle className="h-4 w-4" />
                  <AlertDescription className="text-sm">
                    <strong>What this controls:</strong> Minimum similarity score required for CompreFace to consider a face match as valid.
                    <br />
                    • <strong>Lower values (60-80%):</strong> More matches, but may include false positives
                    <br />
                    • <strong>Recommended (85-90%):</strong> Balanced accuracy
                    <br />
                    • <strong>Higher values (90-95%):</strong> Stricter matching, may miss some valid matches
                    <br />
                    <span className="text-xs text-gray-600 mt-2 block">
                      Current: {(comprefaceRecognition.similarity_threshold * 100).toFixed(1)}%
                      {comprefaceRecognition.similarity_threshold >= 0.90 ? ' (Strict)' :
                       comprefaceRecognition.similarity_threshold >= 0.80 ? ' (Balanced)' : ' (Lenient)'}
                    </span>
                  </AlertDescription>
                </Alert>
              </div>

              {/* Action Buttons */}
              <div className="flex gap-2 pt-4">
                <Button
                  onClick={saveComprefaceRecognitionSettings}
                  disabled={saving}
                  className="flex-1"
                >
                  <Save className="h-4 w-4 mr-2" />
                  Save Recognition Settings
                </Button>
              </div>

              {/* Info about when changes take effect */}
              <Alert className="bg-blue-50 border-blue-200">
                <AlertCircle className="h-4 w-4 text-blue-600" />
                <AlertDescription className="text-blue-800 text-sm">
                  <strong>Note:</strong> Changes apply to new camera configurations. Existing cameras need to be updated to use the new threshold.
                </AlertDescription>
              </Alert>
            </CardContent>
          </Card>

          {/* Logs Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <FileText className="h-5 w-5 text-blue-600" />
                Logs Settings
              </CardTitle>
              <CardDescription>
                Configure which event types to exclude from logs
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-6">
              <div>
                <Label className="text-sm font-semibold mb-3 block">
                  Event Types to Capture
                </Label>
                <p className="text-sm text-gray-500 mb-4">
                  Toggle off event types you don't want to be logged. Changes apply immediately and persist across sessions.
                </p>
                <div className="grid grid-cols-1 gap-3 max-h-80 overflow-y-auto p-3 bg-slate-50 rounded-lg border">
                  {ALL_EVENT_TYPES.map((type) => (
                    <div key={type} className="flex items-center justify-between p-2 bg-white rounded border hover:bg-slate-50 transition-colors">
                      <Label htmlFor={`log-type-${type}`} className="text-sm cursor-pointer flex-1 font-medium">
                        {type}
                      </Label>
                      <Switch
                        id={`log-type-${type}`}
                        checked={!excludedTypes.has(type)}
                        onCheckedChange={() => toggleExcludedType(type)}
                      />
                    </div>
                  ))}
                </div>
                <p className="text-xs text-gray-500 mt-3">
                  <strong>Note:</strong> Excluded events will not be captured or displayed in the Logs page.
                  Currently {excludedTypes.size} event type{excludedTypes.size !== 1 ? 's' : ''} excluded.
                </p>
              </div>

              {/* Info about log categories */}
              <Alert>
                <AlertCircle className="h-4 w-4" />
                <AlertDescription>
                  <strong>Event Categories:</strong><br />
                  <span className="text-xs">
                    • <strong>Motion:</strong> motion_detected<br />
                    • <strong>Face:</strong> face_detected, person_recognized<br />
                    • <strong>Camera:</strong> camera_started, camera_stopped, camera_error<br />
                    • <strong>System:</strong> worker_started, worker_stopped, pipeline_crashed, fps_drop<br />
                    • <strong>Logs:</strong> log, INFO, DEBUG, WARNING, ERROR
                  </span>
                </AlertDescription>
              </Alert>
            </CardContent>
          </Card>
        </div>
      </div>
    </Layout>
  );
}
