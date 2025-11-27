'use client';

import { useState, useRef } from 'react';
import { useRouter } from 'next/navigation';
import Link from 'next/link';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { Switch } from '@/components/ui/switch';
import { Badge } from '@/components/ui/badge';
import { ArrowLeft, Save, Loader2, Upload, X, Image as ImageIcon, User } from 'lucide-react';
import { Header } from '@/components/header';
import apiClient from '@/lib/api';

const MAX_IMAGES = 5;

interface ImageFile {
  file: File;
  preview: string;
}

export default function AddProfilePage() {
  const router = useRouter();
  const fileInputRef = useRef<HTMLInputElement>(null);

  const [saving, setSaving] = useState(false);
  const [images, setImages] = useState<ImageFile[]>([]);
  const [tags, setTags] = useState<string[]>([]);
  const [tagInput, setTagInput] = useState('');

  const [formData, setFormData] = useState({
    name: '',
    compreface_subject_id: '',
    description: '',
    email: '',
    phone: '',
    alert_on_recognition: false,
  });

  const handleImageSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || []);
    const remainingSlots = MAX_IMAGES - images.length;
    const filesToAdd = files.slice(0, remainingSlots);

    const newImages: ImageFile[] = filesToAdd.map((file) => ({
      file,
      preview: URL.createObjectURL(file),
    }));

    setImages([...images, ...newImages]);

    // Reset input
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const handleRemoveImage = (index: number) => {
    const newImages = [...images];
    URL.revokeObjectURL(newImages[index].preview);
    newImages.splice(index, 1);
    setImages(newImages);
  };

  const handleAddTag = () => {
    if (tagInput.trim() && !tags.includes(tagInput.trim())) {
      setTags([...tags, tagInput.trim()]);
      setTagInput('');
    }
  };

  const handleRemoveTag = (tag: string) => {
    setTags(tags.filter((t) => t !== tag));
  };

  const generateSubjectId = () => {
    const randomId = `person_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    setFormData({ ...formData, compreface_subject_id: randomId });
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    // Validation
    if (!formData.name.trim()) {
      alert('Please enter a name');
      return;
    }

    if (!formData.compreface_subject_id.trim()) {
      alert('Please generate or enter a Subject ID');
      return;
    }

    if (images.length === 0) {
      if (!confirm('No images uploaded. Continue without images?')) {
        return;
      }
    }

    setSaving(true);

    try {
      // Create profile
      const profileData = {
        name: formData.name,
        compreface_subject_id: formData.compreface_subject_id,
        description: formData.description || undefined,
        email: formData.email || undefined,
        phone: formData.phone || undefined,
        alert_on_recognition: formData.alert_on_recognition,
        tags: tags.length > 0 ? tags : undefined,
      };

      const profile = await apiClient.createProfile(profileData);

      // Upload images to CompreFace via backend
      if (images.length > 0) {
        for (const image of images) {
          const imageFormData = new FormData();
          imageFormData.append('file', image.file);
          imageFormData.append('subject', formData.compreface_subject_id);

          try {
            await fetch(`http://localhost:8002/api/v1/profiles/${profile.id}/images`, {
              method: 'POST',
              body: imageFormData,
            });
          } catch (error) {
            console.error('Failed to upload image:', error);
          }
        }
      }

      // Clean up previews
      images.forEach((img) => URL.revokeObjectURL(img.preview));

      router.push('/profiles');
    } catch (error) {
      console.error('Failed to create profile:', error);
      alert('Failed to create profile. Please check the form and try again.');
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Add New Profile" description="Create a new person profile with face images" />

      <main className="container mx-auto px-6 py-8 pb-20 max-w-4xl">
        <div className="mb-6">
          <Link href="/profiles">
            <Button variant="ghost" size="sm">
              <ArrowLeft className="h-4 w-4 mr-2" />
              Back to Profiles
            </Button>
          </Link>
        </div>

        <form onSubmit={handleSubmit} className="space-y-6">
          {/* Basic Information */}
          <Card>
            <CardHeader>
              <CardTitle>Basic Information</CardTitle>
              <CardDescription>Profile identification and contact details</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="space-y-2">
                <Label htmlFor="name">Name*</Label>
                <Input
                  id="name"
                  required
                  value={formData.name}
                  onChange={(e) => setFormData({ ...formData, name: e.target.value })}
                  placeholder="e.g., John Doe"
                />
              </div>

              <div className="space-y-2">
                <Label htmlFor="subject_id">CompreFace Subject ID*</Label>
                <div className="flex gap-2">
                  <Input
                    id="subject_id"
                    required
                    value={formData.compreface_subject_id}
                    onChange={(e) => setFormData({ ...formData, compreface_subject_id: e.target.value })}
                    placeholder="Unique identifier for CompreFace"
                  />
                  <Button type="button" variant="outline" onClick={generateSubjectId}>
                    Generate
                  </Button>
                </div>
                <p className="text-xs text-muted-foreground">
                  This ID is used to identify the person in CompreFace
                </p>
              </div>

              <div className="space-y-2">
                <Label htmlFor="description">Description</Label>
                <Textarea
                  id="description"
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  placeholder="Optional description or notes"
                  rows={3}
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div className="space-y-2">
                  <Label htmlFor="email">Email</Label>
                  <Input
                    id="email"
                    type="email"
                    value={formData.email}
                    onChange={(e) => setFormData({ ...formData, email: e.target.value })}
                    placeholder="email@example.com"
                  />
                </div>

                <div className="space-y-2">
                  <Label htmlFor="phone">Phone</Label>
                  <Input
                    id="phone"
                    type="tel"
                    value={formData.phone}
                    onChange={(e) => setFormData({ ...formData, phone: e.target.value })}
                    placeholder="+1234567890"
                  />
                </div>
              </div>

              <div className="flex items-center justify-between">
                <div>
                  <Label htmlFor="alert">Alert on Recognition</Label>
                  <p className="text-sm text-muted-foreground">Send alerts when this person is detected</p>
                </div>
                <Switch
                  id="alert"
                  checked={formData.alert_on_recognition}
                  onCheckedChange={(checked) =>
                    setFormData({ ...formData, alert_on_recognition: checked })
                  }
                />
              </div>
            </CardContent>
          </Card>

          {/* Tags */}
          <Card>
            <CardHeader>
              <CardTitle>Tags</CardTitle>
              <CardDescription>Add labels for organization and filtering</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="flex gap-2">
                <Input
                  value={tagInput}
                  onChange={(e) => setTagInput(e.target.value)}
                  onKeyPress={(e) => {
                    if (e.key === 'Enter') {
                      e.preventDefault();
                      handleAddTag();
                    }
                  }}
                  placeholder="Enter a tag and press Enter"
                />
                <Button type="button" variant="outline" onClick={handleAddTag}>
                  Add Tag
                </Button>
              </div>

              {tags.length > 0 && (
                <div className="flex flex-wrap gap-2">
                  {tags.map((tag) => (
                    <Badge key={tag} variant="secondary" className="gap-1">
                      {tag}
                      <X
                        className="h-3 w-3 cursor-pointer"
                        onClick={() => handleRemoveTag(tag)}
                      />
                    </Badge>
                  ))}
                </div>
              )}
            </CardContent>
          </Card>

          {/* Face Images */}
          <Card>
            <CardHeader>
              <CardTitle>Face Images</CardTitle>
              <CardDescription>
                Upload up to {MAX_IMAGES} images for better recognition accuracy (recommended)
              </CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
              <div className="text-center">
                <input
                  ref={fileInputRef}
                  type="file"
                  accept="image/*"
                  multiple
                  onChange={handleImageSelect}
                  className="hidden"
                />

                <Button
                  type="button"
                  variant="outline"
                  onClick={() => fileInputRef.current?.click()}
                  disabled={images.length >= MAX_IMAGES}
                  className="w-full"
                >
                  <Upload className="h-4 w-4 mr-2" />
                  Upload Images ({images.length}/{MAX_IMAGES})
                </Button>

                <p className="text-xs text-muted-foreground mt-2">
                  Accepted formats: JPG, PNG, GIF. Max {MAX_IMAGES} images.
                </p>
              </div>

              {images.length > 0 && (
                <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                  {images.map((image, index) => (
                    <div key={index} className="relative group">
                      <div className="aspect-square rounded-lg overflow-hidden border-2 border-gray-200 dark:border-gray-700">
                        <img
                          src={image.preview}
                          alt={`Preview ${index + 1}`}
                          className="w-full h-full object-cover"
                        />
                      </div>
                      <Button
                        type="button"
                        variant="destructive"
                        size="icon"
                        className="absolute top-2 right-2 opacity-0 group-hover:opacity-100 transition-opacity"
                        onClick={() => handleRemoveImage(index)}
                      >
                        <X className="h-4 w-4" />
                      </Button>
                      <div className="absolute bottom-2 left-2 bg-black/50 text-white text-xs px-2 py-1 rounded">
                        Image {index + 1}
                      </div>
                    </div>
                  ))}
                </div>
              )}

              {images.length === 0 && (
                <div className="border-2 border-dashed border-gray-300 dark:border-gray-700 rounded-lg p-12 text-center">
                  <ImageIcon className="h-12 w-12 text-gray-400 mx-auto mb-4" />
                  <p className="text-gray-600 dark:text-gray-400">
                    No images uploaded yet
                  </p>
                  <p className="text-sm text-muted-foreground mt-1">
                    Upload face images for better recognition
                  </p>
                </div>
              )}
            </CardContent>
          </Card>

          {/* Actions */}
          <div className="flex justify-end gap-4">
            <Link href="/profiles">
              <Button type="button" variant="outline">
                Cancel
              </Button>
            </Link>
            <Button type="submit" disabled={saving}>
              {saving ? (
                <>
                  <Loader2 className="h-4 w-4 mr-2 animate-spin" />
                  Creating...
                </>
              ) : (
                <>
                  <Save className="h-4 w-4 mr-2" />
                  Create Profile
                </>
              )}
            </Button>
          </div>
        </form>
      </main>
    </div>
  );
}
