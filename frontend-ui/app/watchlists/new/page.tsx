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
import { ArrowLeft, Save, Loader2, Users } from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';
import apiClient from '@/lib/api';
import type { CreateWatchlistRequest, Profile } from '@/lib/types';

export default function AddWatchlistPage() {
  const router = useRouter();
  const [loading, setLoading] = useState(false);
  const [profiles, setProfiles] = useState<Profile[]>([]);
  const [loadingProfiles, setLoadingProfiles] = useState(true);
  const [selectedProfiles, setSelectedProfiles] = useState<string[]>([]);
  const [formData, setFormData] = useState<CreateWatchlistRequest>({
    name: '',
    description: '',
    color: '#3B82F6',
    icon: '🛡️',
    enable_alerts: false,
    alert_on_cameras: [],
    alert_cooldown_minutes: 5,
    webhook_url: '',
    webhook_enabled: false,
    priority: 0,
  });

  useEffect(() => {
    loadProfiles();
  }, []);

  const loadProfiles = async () => {
    try {
      setLoadingProfiles(true);
      const data = await apiClient.getProfiles({ is_active: true });
      setProfiles(data);
    } catch (error) {
      console.error('Failed to load profiles:', error);
    } finally {
      setLoadingProfiles(false);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);

    try {
      // Create watchlist
      const watchlist = await apiClient.createWatchlist(formData);

      // Add selected profiles to watchlist
      for (const profileId of selectedProfiles) {
        try {
          await apiClient.addProfileToWatchlist(watchlist.id, profileId);
        } catch (error) {
          console.error(`Failed to add profile ${profileId}:`, error);
        }
      }

      router.push('/watchlists');
    } catch (error) {
      console.error('Failed to create watchlist:', error);
      alert('Failed to create watchlist. Please check the form and try again.');
    } finally {
      setLoading(false);
    }
  };

  const toggleProfile = (profileId: string) => {
    setSelectedProfiles((prev) =>
      prev.includes(profileId)
        ? prev.filter((id) => id !== profileId)
        : [...prev, profileId]
    );
  };

  return (
    <Layout>
      <PageHeader title="Create Watchlist" description="Set up a new watchlist for profile grouping and alerts" />

      <main className="container mx-auto px-6 py-8 max-w-5xl">
        <div className="mb-6">
          <Link href="/watchlists">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Watchlists
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

              <div className="grid grid-cols-2 gap-4">
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

          {/* Profile Assignment */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Users className="h-5 w-5" />
                Assign Profiles
              </CardTitle>
              <CardDescription>
                Select profiles to add to this watchlist ({selectedProfiles.length} selected)
              </CardDescription>
            </CardHeader>
            <CardContent>
              {loadingProfiles ? (
                <div className="text-center py-8">
                  <Loader2 className="h-8 w-8 animate-spin mx-auto text-gray-400" />
                  <p className="text-sm text-gray-500 mt-2">Loading profiles...</p>
                </div>
              ) : profiles.length === 0 ? (
                <div className="text-center py-8">
                  <p className="text-gray-500">No active profiles available</p>
                </div>
              ) : (
                <div className="space-y-2 max-h-96 overflow-y-auto">
                  {profiles.map((profile) => (
                    <div
                      key={profile.id}
                      className={`flex items-center gap-3 p-3 rounded-lg border cursor-pointer transition-all ${
                        selectedProfiles.includes(profile.id)
                          ? 'border-blue-500 bg-blue-50 dark:bg-blue-900/20'
                          : 'border-gray-200 hover:border-gray-300 hover:bg-gray-50 dark:hover:bg-gray-800'
                      }`}
                      onClick={() => toggleProfile(profile.id)}
                    >
                      <input
                        type="checkbox"
                        checked={selectedProfiles.includes(profile.id)}
                        onChange={() => toggleProfile(profile.id)}
                        className="h-4 w-4"
                      />
                      <div className="flex-1">
                        <p className="font-medium">{profile.name}</p>
                        <p className="text-xs text-gray-500">
                          {profile.compreface_subject_id} • {profile.total_detections} detections
                        </p>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href="/watchlists">
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
                  Create Watchlist
                </>
              )}
            </Button>
          </div>
        </form>
      </main>
    </Layout>
  );
}
