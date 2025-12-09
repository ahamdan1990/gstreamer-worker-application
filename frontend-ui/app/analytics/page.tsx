'use client';

import { Card, CardContent } from '@/components/ui/card';
import { TrendingUp } from 'lucide-react';
import { Layout, PageHeader } from '@/components/layout';

export default function AnalyticsPage() {
  return (
    <Layout>
      <PageHeader title="Analytics" description="Camera analytics and insights" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <TrendingUp className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Analytics Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              View camera performance metrics, motion detection trends, and more
            </p>
          </CardContent>
        </Card>
      </main>
    </Layout>
  );
}
