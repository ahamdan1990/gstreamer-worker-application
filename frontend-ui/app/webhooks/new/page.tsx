'use client';

import { useState, useEffect } from 'react';
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
import { Layout, PageHeader } from '@/components/layout';
import apiClient from '@/lib/api';
import type { Camera, Watchlist } from '@/lib/types';

export default function AddWebhookPage() {
  const router = useRouter();
  const [loading, setLoading] = useState(false);
  const [cameras, setCameras] = useState<Camera[]>([]);
  const [watchlists, setWatchlists] = useState<Watchlist[]>([]);
  const [selectedCameras, setSelectedCameras] = useState<string[]>([]);
  const [selectedWatchlists, setSelectedWatchlists] = useState<string[]>([]);
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    url: '',
    method: 'POST',
    is_active: true,
    auth_type: '',
    auth_header: '',
    auth_value: '',
    event_types: [] as string[],
    headers: {} as Record<string, string>,
    timeout_seconds: 30,
    retry_enabled: true,
    retry_max_attempts: 3,
    retry_backoff_seconds: 5,
  });

  useEffect(() => {
    loadCameras();
    loadWatchlists();
  }, []);

  const loadCameras = async () => {
    try {
      const data = await apiClient.getCameras();
      setCameras(data);
    } catch (error) {
      console.error('Failed to load cameras:', error);
    }
  };

  const loadWatchlists = async () => {
    try {
      const data = await apiClient.getWatchlists({ is_active: true });
      setWatchlists(data);
    } catch (error) {
      console.error('Failed to load watchlists:', error);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);

    try {
      await apiClient.createWebhook({
        ...formData,
        camera_ids: selectedCameras,
        watchlist_ids: selectedWatchlists,
      });

      router.push('/webhooks');
    } catch (error) {
      console.error('Failed to create webhook:', error);
      alert('Failed to create webhook. Please check the form and try again.');
    } finally {
      setLoading(false);
    }
  };

  const eventTypes = [
    'person.recognized',
    'person.new',
    'motion.detected',
    'face.detected',
    'camera.online',
    'camera.offline',
    'camera.error',
  ];

  const toggleEventType = (eventType: string) => {
    setFormData((prev) => ({
      ...prev,
      event_types: prev.event_types.includes(eventType)
        ? prev.event_types.filter((et) => et !== eventType)
        : [...prev.event_types, eventType],
    }));
  };

  const toggleCamera = (cameraId: string) => {
    setSelectedCameras((prev) =>
      prev.includes(cameraId) ? prev.filter((id) => id !== cameraId) : [...prev, cameraId]
    );
  };

  const toggleWatchlist = (watchlistId: string) => {
    setSelectedWatchlists((prev) =>
      prev.includes(watchlistId) ? prev.filter((id) => id !== watchlistId) : [...prev, watchlistId]
    );
  };

  return (
    <Layout>
      <PageHeader title="Create Webhook" description="Set up a new webhook for event notifications" />

      <main className="container mx-auto px-6 py-8 max-w-5xl">
        <div className="mb-6">
          <Link href="/webhooks">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Webhooks
            </Button>
          </Link>
        </div>

        <form onSubmit={handleSubmit} className="space-y-6">
          {/* Basic Information */}
          <Card>
            <CardHeader>
              <CardTitle>Basic Information</CardTitle>
              <CardDescription>Webhook name, URL, and request method</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="space-y-2">
                <Label htmlFor="name">Webhook Name*</Label>
                <Input
                  id="name"
                  required
                  value={formData.name}
                  onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                  placeholder="e.g., Slack Notifications"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  placeholder="Describe the webhook purpose"
                  rows={2}
                />
              </div>

              <div className="grid grid-cols-4 gap-4">
                <div className="col-span-3 space-y-2">
                  <Label htmlFor="url">Webhook URL*</Label>
                  <Input
                    id="url"
                    type="url"
                    required
                    value={formData.url}
                    onChange={(e) => setFormData({ ...formData, url: e.target.value })}
                    placeholder="https://your-webhook-endpoint.com"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="method">Method</Label>
                  <Select
                    value={formData.method}
                    onValueChange={(value) => setFormData({ ...formData, method: value })}
                  >
                    <SelectTrigger id="method">
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="POST">POST</SelectItem>
                      <SelectItem value="PUT">PUT</SelectItem>
                      <SelectItem value="PATCH">PATCH</SelectItem>
                    </SelectContent>
                  </Select>
                </div>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="is_active">Active Status</Label>
                  <p className="text-sm text-muted-foreground">Enable or disable this webhook</p>
                </div>
                <Switch
                  id="is_active"
                  checked={formData.is_active}
                  onCheckedChange={(checked) => setFormData({ ...formData, is_active: checked })}
                />
              </div>
            </CardContent>
          </Card>

          {/* Event Types */}
          <Card>
            <CardHeader>
              <CardTitle>Event Types</CardTitle>
              <CardDescription>Select which events should trigger this webhook</CardDescription>
            </CardHeader>
            <CardContent>
              <div className="space-y-2">
                {eventTypes.map((eventType) => (
                  <div
                    key={eventType}
                    className={`flex items-center gap-3 p-3 rounded-lg border cursor-pointer transition-all ${
                      formData.event_types.includes(eventType)
                        ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/20'
                        : 'border-gray-200 hover:border-gray-300 hover:bg-gray-50'
                    }`}
                    onClick={() => toggleEventType(eventType)}
                  >
                    <input
                      type="checkbox"
                      checked={formData.event_types.includes(eventType)}
                      onChange={() => toggleEventType(eventType)}
                      className="h-4 w-4"
                    />
                    <span className="font-medium">{eventType}</span>
                  </div>
                ))}
              </div>
            </CardContent>
          </Card>

          {/* Filters */}
          <Card>
            <CardHeader>
              <CardTitle>Filters (Optional)</CardTitle>
              <CardDescription>Limit webhook to specific cameras and watchlists</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              {/* Camera Filter */}
              <div className="space-y-2">
                <Label>Cameras ({selectedCameras.length} selected)</Label>
                <p className="text-xs text-muted-foreground">Leave empty to receive events from all cameras</p>
                <div className="grid grid-cols-2 gap-2 max-h-48 overflow-y-auto">
                  {cameras.map((camera) => (
                    <div
                      key={camera.id}
                      className={`flex items-center gap-2 p-2 rounded border cursor-pointer transition-all ${
                        selectedCameras.includes(camera.camera_id)
                          ? 'border-blue-500 bg-blue-50'
                          : 'border-gray-200 hover:border-gray-300'
                      }`}
                      onClick={() => toggleCamera(camera.camera_id)}
                    >
                      <input
                        type="checkbox"
                        checked={selectedCameras.includes(camera.camera_id)}
                        onChange={() => toggleCamera(camera.camera_id)}
                        className="h-3 w-3"
                      />
                      <span className="text-sm">{camera.name}</span>
                    </div>
                  ))}
                </div>
              </div>

              {/* Watchlist Filter */}
              <div className="space-y-2">
                <Label>Watchlists ({selectedWatchlists.length} selected)</Label>
                <p className="text-xs text-muted-foreground">Leave empty to receive events from all watchlists</p>
                <div className="grid grid-cols-2 gap-2 max-h-48 overflow-y-auto">
                  {watchlists.map((watchlist) => (
                    <div
                      key={watchlist.id}
                      className={`flex items-center gap-2 p-2 rounded border cursor-pointer transition-all ${
                        selectedWatchlists.includes(watchlist.id)
                          ? 'border-blue-500 bg-blue-50'
                          : 'border-gray-200 hover:border-gray-300'
                      }`}
                      onClick={() => toggleWatchlist(watchlist.id)}
                    >
                      <input
                        type="checkbox"
                        checked={selectedWatchlists.includes(watchlist.id)}
                        onChange={() => toggleWatchlist(watchlist.id)}
                        className="h-3 w-3"
                      />
                      <span className="text-sm">{watchlist.name}</span>
                    </div>
                  ))}
                </div>
              </div>
            </CardContent>
          </Card>

          {/* Authentication */}
          <Card>
            <CardHeader>
              <CardTitle>Authentication (Optional)</CardTitle>
              <CardDescription>Configure authentication for webhook requests</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="grid grid-cols-3 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="auth_type">Auth Type</Label>
                  <Select
                    value={formData.auth_type}
                    onValueChange={(value) => setFormData({ ...formData, auth_type: value })}
                  >
                    <SelectTrigger id="auth_type">
                      <SelectValue placeholder="None" />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="">None</SelectItem>
                      <SelectItem value="Bearer">Bearer Token</SelectItem>
                      <SelectItem value="Basic">Basic Auth</SelectItem>
                      <SelectItem value="ApiKey">API Key</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                {formData.auth_type && (
                  <>
                    <div className="space-y-2">
                      <Label htmlFor="auth_header">Header Name</Label>
                      <Input
                        id="auth_header"
                        value={formData.auth_header}
                        onChange={(e) => setFormData({ ...formData, auth_header: e.target.value })}
                        placeholder="e.g., Authorization"
                      />
                    </div>

                    <div className="space-y-2">
                      <Label htmlFor="auth_value">Auth Value</Label>
                      <Input
                        id="auth_value"
                        type="password"
                        value={formData.auth_value}
                        onChange={(e) => setFormData({ ...formData, auth_value: e.target.value })}
                        placeholder="Token or credentials"
                      />
                    </div>
                  </>
                )}
              </div>
            </CardContent>
          </Card>

          {/* Retry Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Retry Configuration</CardTitle>
              <CardDescription>Configure retry behavior for failed webhook calls</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="retry_enabled">Enable Retry</Label>
                  <p className="text-sm text-muted-foreground">Automatically retry failed webhook calls</p>
                </div>
                <Switch
                  id="retry_enabled"
                  checked={formData.retry_enabled}
                  onCheckedChange={(checked) => setFormData({ ...formData, retry_enabled: checked })}
                />
              </div>

              {formData.retry_enabled && (
                <div className="grid grid-cols-3 gap-4 pt-4 border-t">
                  <div className="space-y-2">
                    <Label htmlFor="timeout">Timeout (seconds)</Label>
                    <Input
                      id="timeout"
                      type="number"
                      min="1"
                      value={formData.timeout_seconds}
                      onChange={(e) => setFormData({ ...formData, timeout_seconds: parseInt(e.target.value) })}
                    />
                  </div>

                  <div className="space-y-2">
                    <Label htmlFor="max_attempts">Max Attempts</Label>
                    <Input
                      id="max_attempts"
                      type="number"
                      min="1"
                      value={formData.retry_max_attempts}
                      onChange={(e) => setFormData({ ...formData, retry_max_attempts: parseInt(e.target.value) })}
                    />
                  </div>

                  <div className="space-y-2">
                    <Label htmlFor="backoff">Backoff (seconds)</Label>
                    <Input
                      id="backoff"
                      type="number"
                      min="1"
                      value={formData.retry_backoff_seconds}
                      onChange={(e) =>
                        setFormData({ ...formData, retry_backoff_seconds: parseInt(e.target.value) })
                      }
                    />
                  </div>
                </div>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href="/webhooks">
              <Button type="button" variant="outline">
                Cancel
              </Button>
            </Link>
            <Button type="submit" disabled={loading}>
              {loading ? (
                <>
                  <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                  Creating...
                </>
              ) : (
                <>
                  <Save className="h-4 w-4 mr-2" />
                  Create Webhook
                </>
              )}
            </Button>
          </div>
        </form>
      </main>
    </Layout>
  );
}
