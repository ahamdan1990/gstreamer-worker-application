'use client';

import { useState, useEffect } from 'react';
import { useParams, useRouter } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { Switch } from '@/components/ui/switch';
import { ArrowLeft, Save, Loader2 } from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';
import apiClient from '@/lib/api';
import type { UpdateWatchlistRequest, Watchlist } from '@/lib/types';

export default function EditWatchlistPage() {
  const params = useParams();
  const router = useRouter();
  const watchlistId = params.id as string;

  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [formData, setFormData] = useState<UpdateWatchlistRequest>({
    name: '',
    description: '',
    color: '#3B82F6',
    icon: '🛡️',
    is_active: true,
    enable_alerts: false,
    alert_on_cameras: [],
    alert_cooldown_minutes: 5,
    webhook_url: '',
    webhook_enabled: false,
    priority: 0,
  });

  useEffect(() => {
    loadWatchlist();
  }, [watchlistId]);

  const loadWatchlist = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getWatchlist(watchlistId);
      setFormData({
        name: data.name,
        description: data.description,
        color: data.color,
        icon: data.icon,
        is_active: data.is_active,
        enable_alerts: data.enable_alerts,
        alert_on_cameras: data.alert_on_cameras,
        alert_cooldown_minutes: data.alert_cooldown_minutes,
        webhook_url: data.webhook_url,
        webhook_enabled: data.webhook_enabled,
        priority: data.priority,
      });
    } catch (error) {
      console.error('Failed to load watchlist:', error);
      alert('Failed to load watchlist');
      router.push('/watchlists');
    } finally {
      setLoading(false);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setSaving(true);

    try {
      await apiClient.updateWatchlist(watchlistId, formData);
      router.push(`/watchlists/${watchlistId}`);
    } catch (error) {
      console.error('Failed to update watchlist:', error);
      alert('Failed to update watchlist. Please check the form and try again.');
    } finally {
      setSaving(false);
    }
  };

  if (loading) {
    return (
      <Layout>
        <div className="flex items-center justify-center min-h-screen">
          <div className="text-center">
            <Loader2 className="h-12 w-12 animate-spin mx-auto text-blue-600" />
            <p className="mt-4 text-gray-600">Loading watchlist...</p>
          </div>
        </div>
      </Layout>
    );
  }

  return (
    <Layout>
      <PageHeader title="Edit Watchlist" description="Update watchlist settings and configuration" />

      <main className="container mx-auto px-6 py-8 max-w-5xl">
        <div className="mb-6">
          <Link href={`/watchlists/${watchlistId}`}>
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Watchlist
            </Button>
          </Link>
        </div>

        <form onSubmit={handleSubmit} className="space-y-6">
          {/* Basic Information */}
          <Card>
            <CardHeader>
              <CardTitle>Basic Information</CardTitle>
              <CardDescription>Watchlist name, description, and appearance</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="grid grid-cols-2 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="name">Watchlist Name*</Label>
                  <Input
                    id="name"
                    required
                    value={formData.name}
                    onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                    placeholder="e.g., VIP Guests"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="icon">Icon (Emoji)</Label>
                  <Input
                    id="icon"
                    value={formData.icon}
                    onChange={(e) => setFormData({ ...formData, icon: e.target.value })}
                    placeholder="🛡️"
                    maxLength={2}
                  />
                </div>
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  placeholder="Describe the purpose of this watchlist"
                  rows={3}
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="color">Color</Label>
                <div className="flex gap-4 items-center">
                  <Input
                    id="color"
                    type="color"
                    value={formData.color}
                    onChange={(e) => setFormData({ ...formData, color: e.target.value })}
                    className="w-20 h-10"
                  />
                  <span className="text-sm text-muted-foreground">{formData.color}</span>
                </div>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="is_active">Active Status</Label>
                  <p className="text-sm text-muted-foreground">Enable or disable this watchlist</p>
                </div>
                <Switch
                  id="is_active"
                  checked={formData.is_active}
                  onCheckedChange={(checked) => setFormData({ ...formData, is_active: checked })}
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="priority">Priority Level</Label>
                <Input
                  id="priority"
                  type="number"
                  min="0"
                  max="10"
                  value={formData.priority}
                  onChange={(e) => setFormData({ ...formData, priority: parseInt(e.target.value) })}
                />
                <p className="text-xs text-muted-foreground">0 = Normal, 10 = Highest</p>
              </div>
            </CardContent>
          </Card>

          {/* Alert Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Alert Configuration</CardTitle>
              <CardDescription>Configure alert settings for this watchlist</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="enable_alerts">Enable Alerts</Label>
                  <p className="text-sm text-muted-foreground">Send alerts when watchlist members are detected</p>
                </div>
                <Switch
                  id="enable_alerts"
                  checked={formData.enable_alerts}
                  onCheckedChange={(checked) => setFormData({ ...formData, enable_alerts: checked })}
                />
              </div>

              {formData.enable_alerts && (
                <div className="space-y-4 pt-4 border-t">
                  <div className="space-y-2">
                    <Label htmlFor="alert_cooldown">Alert Cooldown (minutes)</Label>
                    <Input
                      id="alert_cooldown"
                      type="number"
                      min="0"
                      value={formData.alert_cooldown_minutes}
                      onChange={(e) =>
                        setFormData({ ...formData, alert_cooldown_minutes: parseInt(e.target.value) })
                      }
                    />
                    <p className="text-xs text-muted-foreground">Minimum time between alerts for same person</p>
                  </div>
                </div>
              )}
            </CardContent>
          </Card>

          {/* Webhook Configuration */}
          <Card>
            <CardHeader>
              <CardTitle>Webhook Integration</CardTitle>
              <CardDescription>Send detection events to external webhook</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="webhook_enabled">Enable Webhook</Label>
                  <p className="text-sm text-muted-foreground">POST detection events to webhook URL</p>
                </div>
                <Switch
                  id="webhook_enabled"
                  checked={formData.webhook_enabled}
                  onCheckedChange={(checked) => setFormData({ ...formData, webhook_enabled: checked })}
                />
              </div>

              {formData.webhook_enabled && (
                <div className="space-y-2 pt-4 border-t">
                  <Label htmlFor="webhook_url">Webhook URL</Label>
                  <Input
                    id="webhook_url"
                    type="url"
                    value={formData.webhook_url}
                    onChange={(e) => setFormData({ ...formData, webhook_url: e.target.value })}
                    placeholder="https://your-webhook-endpoint.com/alerts"
                  />
                </div>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href={`/watchlists/${watchlistId}`}>
              <Button type="button" variant="outline">
                Cancel
              </Button>
            </Link>
            <Button type="submit" disabled={saving}>
              {saving ? (
                <>
                  <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                  Saving...
                </>
              ) : (
                <>
                  <Save className="h-4 w-4 mr-2" />
                  Save Changes
                </>
              )}
            </Button>
          </div>
        </form>
      </main>
    </Layout>
  );
}
