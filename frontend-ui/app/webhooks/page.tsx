'use client';

import { useEffect, useState } from 'react';
import Link from 'next/link';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import {
  Webhook as WebhookIcon,
  Plus,
  Search,
  Eye,
  Edit,
  Trash2,
  Activity,
  CheckCircle,
  XCircle,
  Clock,
  Send,
  AlertTriangle,
  TrendingUp,
} from 'lucide-react';
import { Layout } from '@/components/layout';
import apiClient from '@/lib/api';
import type { Webhook } from '@/lib/types';

export default function WebhooksPage() {
  const [webhooks, setWebhooks] = useState<Webhook[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [filterActive, setFilterActive] = useState<boolean | undefined>(undefined);
  const [testingWebhook, setTestingWebhook] = useState<string | null>(null);

  useEffect(() => {
    loadWebhooks();
  }, []);

  const loadWebhooks = async () => {
    try {
      setLoading(true);
      const data = await apiClient.getWebhooks({
        is_active: filterActive,
      });

      // Filter by search query client-side
      let filtered = data;
      if (searchQuery) {
        const query = searchQuery.toLowerCase();
        filtered = data.filter(
          (w) =>
            w.name.toLowerCase().includes(query) ||
            w.description?.toLowerCase().includes(query) ||
            w.url.toLowerCase().includes(query)
        );
      }

      setWebhooks(filtered);
    } catch (error) {
      console.error('Failed to load webhooks:', error);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    const debounce = setTimeout(() => {
      loadWebhooks();
    }, 300);
    return () => clearTimeout(debounce);
  }, [searchQuery, filterActive]);

  const handleDeleteWebhook = async (webhookId: string) => {
    if (!confirm('Are you sure you want to delete this webhook?')) return;

    try {
      await apiClient.deleteWebhook(webhookId);
      loadWebhooks();
    } catch (error) {
      console.error('Failed to delete webhook:', error);
      alert('Failed to delete webhook');
    }
  };

  const handleToggleActive = async (webhook: Webhook) => {
    try {
      await apiClient.updateWebhook(webhook.id, {
        is_active: !webhook.is_active,
      });
      loadWebhooks();
    } catch (error) {
      console.error('Failed to update webhook:', error);
      alert('Failed to update webhook');
    }
  };

  const handleTestWebhook = async (webhookId: string) => {
    try {
      setTestingWebhook(webhookId);
      const result = await apiClient.testWebhook(webhookId);

      if (result.success) {
        alert(`Webhook test successful! Status: ${result.status_code}`);
      } else {
        alert(`Webhook test failed! Status: ${result.status_code}\n${result.message}`);
      }

      loadWebhooks(); // Refresh to show updated stats
    } catch (error: any) {
      console.error('Failed to test webhook:', error);
      alert(`Webhook test failed: ${error.response?.data?.detail || error.message}`);
    } finally {
      setTestingWebhook(null);
    }
  };

  // Calculate total statistics
  const totalCalls = webhooks.reduce((sum, w) => sum + w.total_calls, 0);
  const totalSuccess = webhooks.reduce((sum, w) => sum + w.successful_calls, 0);
  const totalFailed = webhooks.reduce((sum, w) => sum + w.failed_calls, 0);
  const activeWebhooks = webhooks.filter((w) => w.is_active).length;
  const successRate = totalCalls > 0 ? ((totalSuccess / totalCalls) * 100).toFixed(1) : '0.0';

  return (
    <Layout>
      <div className="container mx-auto py-8 px-4">
        {/* Page Header */}
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-2">
              <WebhookIcon className="h-8 w-8 text-blue-600" />
              Webhooks
            </h1>
            <p className="text-gray-500 mt-1">
              Configure HTTP callbacks for third-party integrations
            </p>
          </div>
          <Link href="/webhooks/new">
            <Button className="flex items-center gap-2">
              <Plus className="h-4 w-4" />
              Create Webhook
            </Button>
          </Link>
        </div>

        {/* Statistics Cards */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Total Webhooks</p>
                  <p className="text-2xl font-bold">{webhooks.length}</p>
                </div>
                <WebhookIcon className="h-8 w-8 text-blue-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Active</p>
                  <p className="text-2xl font-bold">{activeWebhooks}</p>
                </div>
                <Activity className="h-8 w-8 text-green-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Total Calls</p>
                  <p className="text-2xl font-bold">{totalCalls}</p>
                </div>
                <Send className="h-8 w-8 text-purple-500" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="pt-6">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-sm text-gray-500">Success Rate</p>
                  <p className="text-2xl font-bold">{successRate}%</p>
                </div>
                <TrendingUp className="h-8 w-8 text-orange-500" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Filters */}
        <Card className="mb-6">
          <CardContent className="pt-6">
            <div className="flex flex-col md:flex-row gap-4">
              <div className="flex-1 relative">
                <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 h-4 w-4 text-gray-400" />
                <Input
                  placeholder="Search webhooks..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  className="pl-10"
                />
              </div>
              <div className="flex gap-2">
                <Button
                  variant={filterActive === undefined ? 'default' : 'outline'}
                  onClick={() => setFilterActive(undefined)}
                >
                  All
                </Button>
                <Button
                  variant={filterActive === true ? 'default' : 'outline'}
                  onClick={() => setFilterActive(true)}
                >
                  Active
                </Button>
                <Button
                  variant={filterActive === false ? 'default' : 'outline'}
                  onClick={() => setFilterActive(false)}
                >
                  Inactive
                </Button>
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Webhooks Grid */}
        {loading ? (
          <div className="text-center py-12">
            <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-blue-600"></div>
            <p className="mt-2 text-gray-500">Loading webhooks...</p>
          </div>
        ) : webhooks.length === 0 ? (
          <Card>
            <CardContent className="py-12">
              <div className="text-center">
                <WebhookIcon className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                <h3 className="text-lg font-semibold mb-2">No webhooks found</h3>
                <p className="text-gray-500 mb-4">
                  {searchQuery
                    ? 'Try adjusting your search query'
                    : 'Create your first webhook to get started'}
                </p>
                <Link href="/webhooks/new">
                  <Button>
                    <Plus className="h-4 w-4 mr-2" />
                    Create Webhook
                  </Button>
                </Link>
              </div>
            </CardContent>
          </Card>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            {webhooks.map((webhook) => {
              const webhookSuccessRate =
                webhook.total_calls > 0
                  ? ((webhook.successful_calls / webhook.total_calls) * 100).toFixed(1)
                  : '0.0';

              return (
                <Card key={webhook.id} className="hover:shadow-lg transition-shadow">
                  <CardHeader>
                    <div className="flex items-start justify-between">
                      <div className="flex-1">
                        <CardTitle className="flex items-center gap-2">
                          <WebhookIcon className="h-5 w-5" />
                          {webhook.name}
                        </CardTitle>
                        <p className="text-sm text-gray-500 mt-1 line-clamp-2">
                          {webhook.description || 'No description'}
                        </p>
                        <p className="text-xs text-gray-400 mt-1 font-mono truncate">
                          {webhook.method} {webhook.url}
                        </p>
                      </div>
                      <div className="flex gap-1">
                        {webhook.is_active ? (
                          <Badge variant="default" className="bg-green-500">
                            Active
                          </Badge>
                        ) : (
                          <Badge variant="secondary">Inactive</Badge>
                        )}
                      </div>
                    </div>
                  </CardHeader>
                  <CardContent>
                    {/* Statistics */}
                    <div className="grid grid-cols-3 gap-4 mb-4">
                      <div className="text-center">
                        <p className="text-xs text-gray-500">Total</p>
                        <p className="text-lg font-bold">{webhook.total_calls}</p>
                      </div>
                      <div className="text-center">
                        <p className="text-xs text-gray-500 flex items-center justify-center gap-1">
                          <CheckCircle className="h-3 w-3 text-green-500" />
                          Success
                        </p>
                        <p className="text-lg font-bold text-green-600">
                          {webhook.successful_calls}
                        </p>
                      </div>
                      <div className="text-center">
                        <p className="text-xs text-gray-500 flex items-center justify-center gap-1">
                          <XCircle className="h-3 w-3 text-red-500" />
                          Failed
                        </p>
                        <p className="text-lg font-bold text-red-600">
                          {webhook.failed_calls}
                        </p>
                      </div>
                    </div>

                    {/* Success Rate Bar */}
                    <div className="mb-4">
                      <div className="flex justify-between text-xs text-gray-500 mb-1">
                        <span>Success Rate</span>
                        <span className="font-semibold">{webhookSuccessRate}%</span>
                      </div>
                      <div className="w-full bg-gray-200 rounded-full h-2">
                        <div
                          className="bg-green-500 h-2 rounded-full transition-all"
                          style={{ width: `${webhookSuccessRate}%` }}
                        ></div>
                      </div>
                    </div>

                    {/* Event Types */}
                    {webhook.event_types.length > 0 && (
                      <div className="mb-4">
                        <p className="text-xs text-gray-500 mb-2">Event Types:</p>
                        <div className="flex flex-wrap gap-1">
                          {webhook.event_types.map((eventType) => (
                            <Badge key={eventType} variant="outline" className="text-xs">
                              {eventType}
                            </Badge>
                          ))}
                        </div>
                      </div>
                    )}

                    {/* Last Call Info */}
                    {webhook.last_called_at && (
                      <div className="text-xs text-gray-500 mb-4 p-2 bg-gray-50 rounded">
                        <div className="flex items-center gap-2 mb-1">
                          <Clock className="h-3 w-3" />
                          <span>
                            Last called: {new Date(webhook.last_called_at).toLocaleString()}
                          </span>
                        </div>
                        {webhook.last_status_code && (
                          <p>
                            Status:{' '}
                            <span
                              className={
                                webhook.last_status_code >= 200 &&
                                webhook.last_status_code < 300
                                  ? 'text-green-600 font-semibold'
                                  : 'text-red-600 font-semibold'
                              }
                            >
                              {webhook.last_status_code}
                            </span>
                          </p>
                        )}
                        {webhook.last_error && (
                          <p className="text-red-600 text-xs mt-1 truncate">
                            Error: {webhook.last_error}
                          </p>
                        )}
                      </div>
                    )}

                    {/* Auth & Settings */}
                    <div className="flex flex-wrap gap-2 mb-4">
                      {webhook.auth_type && (
                        <Badge variant="outline" className="text-xs">
                          Auth: {webhook.auth_type}
                        </Badge>
                      )}
                      {webhook.retry_enabled && (
                        <Badge variant="outline" className="text-xs">
                          Retry: {webhook.retry_max_attempts}x
                        </Badge>
                      )}
                      <Badge variant="outline" className="text-xs">
                        Timeout: {webhook.timeout_seconds}s
                      </Badge>
                    </div>

                    {/* Actions */}
                    <div className="flex gap-2">
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleTestWebhook(webhook.id)}
                        disabled={testingWebhook === webhook.id}
                        className="flex-1"
                      >
                        {testingWebhook === webhook.id ? (
                          <>
                            <div className="animate-spin rounded-full h-3 w-3 border-b-2 border-gray-600 mr-2"></div>
                            Testing...
                          </>
                        ) : (
                          <>
                            <Send className="h-4 w-4 mr-2" />
                            Test
                          </>
                        )}
                      </Button>
                      <Link href={`/webhooks/${webhook.id}`}>
                        <Button variant="outline" size="sm">
                          <Eye className="h-4 w-4" />
                        </Button>
                      </Link>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleToggleActive(webhook)}
                        title={webhook.is_active ? 'Deactivate' : 'Activate'}
                      >
                        {webhook.is_active ? (
                          <XCircle className="h-4 w-4 text-red-500" />
                        ) : (
                          <CheckCircle className="h-4 w-4 text-green-500" />
                        )}
                      </Button>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleDeleteWebhook(webhook.id)}
                        className="text-red-600 hover:bg-red-50"
                      >
                        <Trash2 className="h-4 w-4" />
                      </Button>
                    </div>
                  </CardContent>
                </Card>
              );
            })}
          </div>
        )}
      </div>
    </Layout>
  );
}
