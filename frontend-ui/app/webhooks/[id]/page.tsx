'use client';

import { useEffect, useState } from 'react';
import { useParams, useRouter } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import {
  ArrowLeft,
  Edit,
  Trash2,
  Activity,
  CheckCircle,
  XCircle,
  Clock,
  Send,
  AlertTriangle,
  TrendingUp,
  Loader2,
} from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';
import type { Webhook } from '@/lib/types';

export default function WebhookViewPage() {
  const params = useParams();
  const router = useRouter();
  const webhookId = params.id as string;

  const [webhook, setWebhook] = useState<Webhook | null>(null);
  const [loading, setLoading] = useState(true);
  const [testing, setTesting] = useState(false);

  useEffect(() => {
    loadWebhook();
  }, [webhookId]);

  const loadWebhook = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getWebhook(webhookId);
      setWebhook(data);
    } catch (error) {
      console.error('Failed to load webhook:', error);
      alert('Failed to load webhook');
      router.push('/webhooks');
    } finally {
      setLoading(false);
    }
  };

  const handleTestWebhook = async () => {
    try {
      setTesting(true);
      const result = await apiClient.testWebhook(webhookId);

      if (result.success) {
        alert(`Webhook test successful!\n\nStatus Code: ${result.status_code}\n${result.message}`);
      } else {
        alert(`Webhook test failed!\n\nStatus Code: ${result.status_code}\n${result.message}`);
      }

      await loadWebhook(); // Reload to show updated stats
    } catch (error: any) {
      console.error('Failed to test webhook:', error);
      alert(`Webhook test failed:\n${error.response?.data?.detail || error.message}`);
    } finally {
      setTesting(false);
    }
  };

  const handleDeleteWebhook = async () => {
    if (!confirm('Are you sure you want to delete this webhook? This action cannot be undone.')) return;

    try {
      await apiClient.deleteWebhook(webhookId);
      router.push('/webhooks');
    } catch (error) {
      console.error('Failed to delete webhook:', error);
      alert('Failed to delete webhook');
    }
  };

  if (loading) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-center">
          <Loader2 className="h-12 w-12 animate-spin mx-auto text-blue-600" />
          <p className="mt-4 text-gray-600">Loading webhook...</p>
        </div>
      </div>
    );
  }

  if (!webhook) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray-600">Webhook not found</p>
          <Link href="/webhooks">
            <Button className="mt-4">Back to Webhooks</Button>
          </Link>
        </div>
      </div>
    );
  }

  const successRate =
    webhook.total_calls > 0 ? ((webhook.successful_calls / webhook.total_calls) * 100).toFixed(1) : '0.0';

  return (
    <div className="min-h-screen bg-gray-50">
      <Header />

      <main className="container mx-auto px-4 py-8">
        {/* Header */}
        <div className="mb-6">
          <Link href="/webhooks">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Webhooks
            </Button>
          </Link>
        </div>

        <div className="flex justify-between items-start mb-6">
          <div>
            <div className="flex items-center gap-3 mb-2">
              <h1 className="text-3xl font-bold text-gray-900">{webhook.name}</h1>
              <Badge variant={webhook.is_active ? 'default' : 'secondary'} className="text-sm">
                {webhook.is_active ? 'Active' : 'Inactive'}
              </Badge>
            </div>
            {webhook.description && <p className="text-gray-600">{webhook.description}</p>}
            <p className="text-sm text-gray-500 font-mono mt-2">{webhook.url}</p>
          </div>
          <div className="flex gap-2">
            <Button variant="outline" onClick={handleTestWebhook} disabled={testing}>
              {testing ? (
                <Loader2 className="h-4 w-4 mr-2 animate-spin" />
              ) : (
                <Send className="h-4 w-4 mr-2" />
              )}
              Test Webhook
            </Button>
            <Link href={`/webhooks/${webhookId}/edit`}>
              <Button variant="outline">
                <Edit className="h-4 w-4 mr-2" />
                Edit
              </Button>
            </Link>
            <Button variant="outline" onClick={handleDeleteWebhook} className="text-red-600 hover:bg-red-50">
              <Trash2 className="h-4 w-4 mr-2" />
              Delete
            </Button>
          </div>
        </div>

        {/* Statistics */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Total Calls</p>
                  <p className="text-2xl font-bold text-gray-900">{webhook.total_calls}</p>
                </div>
                <Activity className="w-8 h-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Successful</p>
                  <p className="text-2xl font-bold text-green-600">{webhook.successful_calls}</p>
                </div>
                <CheckCircle className="w-8 h-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Failed</p>
                  <p className="text-2xl font-bold text-red-600">{webhook.failed_calls}</p>
                </div>
                <XCircle className="w-8 h-8 text-red-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-600">Success Rate</p>
                  <p className="text-2xl font-bold text-gray-900">{successRate}%</p>
                </div>
                <TrendingUp className="w-8 h-8 text-purple-500" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Configuration Details */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
          <Card>
            <CardHeader>
              <CardTitle>Request Configuration</CardTitle>
            </CardHeader>
            <CardContent className="space-y-3">
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">HTTP Method</span>
                <Badge>{webhook.method}</Badge>
              </div>
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Timeout</span>
                <span className="text-sm font-medium">{webhook.timeout_seconds}s</span>
              </div>
              <div>
                <span className="text-sm text-gray-600">URL</span>
                <p className="text-sm font-mono bg-gray-100 p-2 rounded mt-1 break-all">{webhook.url}</p>
              </div>
              {webhook.auth_type && (
                <div className="flex justify-between">
                  <span className="text-sm text-gray-600">Authentication</span>
                  <Badge variant="outline">{webhook.auth_type}</Badge>
                </div>
              )}
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Retry Configuration</CardTitle>
            </CardHeader>
            <CardContent className="space-y-3">
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Retry Enabled</span>
                <Badge variant={webhook.retry_enabled ? 'default' : 'secondary'}>
                  {webhook.retry_enabled ? 'Yes' : 'No'}
                </Badge>
              </div>
              {webhook.retry_enabled && (
                <>
                  <div className="flex justify-between">
                    <span className="text-sm text-gray-600">Max Attempts</span>
                    <span className="text-sm font-medium">{webhook.retry_max_attempts}</span>
                  </div>
                  <div className="flex justify-between">
                    <span className="text-sm text-gray-600">Backoff</span>
                    <span className="text-sm font-medium">{webhook.retry_backoff_seconds}s</span>
                  </div>
                </>
              )}
            </CardContent>
          </Card>
        </div>

        {/* Event Types */}
        <Card className="mb-6">
          <CardHeader>
            <CardTitle>Event Types</CardTitle>
            <CardDescription>Events that trigger this webhook</CardDescription>
          </CardHeader>
          <CardContent>
            {webhook.event_types.length === 0 ? (
              <p className="text-sm text-gray-500">No event types configured</p>
            ) : (
              <div className="flex flex-wrap gap-2">
                {webhook.event_types.map((eventType) => (
                  <Badge key={eventType} variant="outline" className="text-sm">
                    {eventType}
                  </Badge>
                ))}
              </div>
            )}
          </CardContent>
        </Card>

        {/* Filters */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
          <Card>
            <CardHeader>
              <CardTitle>Camera Filter</CardTitle>
              <CardDescription>Cameras triggering this webhook</CardDescription>
            </CardHeader>
            <CardContent>
              {webhook.camera_ids.length === 0 ? (
                <p className="text-sm text-gray-500">All cameras (no filter)</p>
              ) : (
                <div className="flex flex-wrap gap-2">
                  {webhook.camera_ids.map((cameraId) => (
                    <Badge key={cameraId} variant="outline" className="text-sm">
                      {cameraId}
                    </Badge>
                  ))}
                </div>
              )}
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle>Watchlist Filter</CardTitle>
              <CardDescription>Watchlists triggering this webhook</CardDescription>
            </CardHeader>
            <CardContent>
              {webhook.watchlist_ids.length === 0 ? (
                <p className="text-sm text-gray-500">All watchlists (no filter)</p>
              ) : (
                <div className="flex flex-wrap gap-2">
                  {webhook.watchlist_ids.map((watchlistId) => (
                    <Badge key={watchlistId} variant="outline" className="text-sm">
                      {watchlistId}
                    </Badge>
                  ))}
                </div>
              )}
            </CardContent>
          </Card>
        </div>

        {/* Last Call Status */}
        {webhook.last_called_at && (
          <Card>
            <CardHeader>
              <CardTitle>Last Call Status</CardTitle>
            </CardHeader>
            <CardContent className="space-y-3">
              <div className="flex justify-between">
                <span className="text-sm text-gray-600">Last Called</span>
                <span className="text-sm font-medium">{new Date(webhook.last_called_at).toLocaleString()}</span>
              </div>
              {webhook.last_status_code && (
                <div className="flex justify-between">
                  <span className="text-sm text-gray-600">Status Code</span>
                  <Badge
                    variant={
                      webhook.last_status_code >= 200 && webhook.last_status_code < 300 ? 'default' : 'destructive'
                    }
                  >
                    {webhook.last_status_code}
                  </Badge>
                </div>
              )}
              {webhook.last_error && (
                <div>
                  <span className="text-sm text-gray-600">Last Error</span>
                  <p className="text-sm text-red-600 font-mono bg-red-50 p-2 rounded mt-1">{webhook.last_error}</p>
                </div>
              )}
            </CardContent>
          </Card>
        )}
      </main>
    </div>
  );
}
